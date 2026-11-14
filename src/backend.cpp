#include "backend.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

#include "portalfilepicker.h"

namespace {
// A selection (or a section) shorter than this is treated as a point.
constexpr double kMinimumSpan = 0.05;
constexpr int kUndoDepth = 100;
} // namespace

Backend::Backend(Decoder *decoder, FilePicker *picker, QObject *parent) : QObject(parent) {
    m_decoder = decoder ? decoder : new Decoder({}, {}, this);
    if (!decoder)
        connect(m_decoder, &Decoder::destroyed, this, [this] { m_decoder = nullptr; });
    m_picker = picker ? picker : new PortalFilePicker(this);

    connect(m_decoder, &Decoder::progressed, this, [this](double fraction) {
        m_progress = fraction;
        emit loadingChanged();
    });
    connect(m_decoder, &Decoder::decoded, this, [this](const QString &path, double seconds) {
        if (path != m_filePath)
            return; // A newer open superseded this decode.
        m_frames = m_decoder->takeFrames();
        m_player.setSource(m_frames.data(), qint64(m_frames.size() / Player::kChannels));
        m_duration = seconds;
        m_peaks = buildPeaks(m_frames.data(), qint64(m_frames.size() / Player::kChannels),
                             Player::kChannels);
        m_loading = false;
        m_progress = 1;
        m_playhead = 0;
        loadSidecarState();
        applyLoopToPlayer();
        emit loadingChanged();
        emit trackChanged();
        emit playheadChanged();
        emit sectionsChanged();
    });
    connect(m_decoder, &Decoder::failed, this, [this](const QString &message) {
        if (!m_loading)
            return; // A cancelled request is allowed to fail quietly.
        m_loading = false;
        m_frames.clear();
        m_peaks.clear();
        m_duration = 0;
        setError(message);
        emit loadingChanged();
        emit trackChanged();
    });

    connect(m_picker, &FilePicker::openSelected, this, &Backend::openPath);
    connect(m_picker, &FilePicker::failed, this, &Backend::setError);

    connect(&m_player, &Player::playingChanged, this,
            [this] { emit playingChanged(); });
    connect(&m_player, &Player::speedChanged, this,
            [this](double) { emit speedChanged(); scheduleSave(); });
    connect(&m_player, &Player::volumeChanged, this,
            [this](double) { emit volumeChanged(); scheduleSave(); });
    connect(&m_player, &Player::errorOccurred, this, &Backend::setError);
    connect(&m_metronome, &Metronome::errorOccurred, this, &Backend::setError);

    m_tick.setInterval(33);
    connect(&m_tick, &QTimer::timeout, this,
            [this] { setPlayhead(m_player.inputPosition()); });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(600);
    connect(&m_saveTimer, &QTimer::timeout, this, &Backend::saveNow);
}

Backend::~Backend() {
    if (m_persist && m_dirty && !m_filePath.isEmpty())
        saveNow();
}

// ---------------------------------------------------------------- state ---

QString Backend::fileName() const { return m_fileName; }
QString Backend::filePath() const { return m_filePath; }
double Backend::duration() const { return m_duration; }
bool Backend::ready() const { return !m_filePath.isEmpty() && !m_loading && m_duration > 0; }
bool Backend::loading() const { return m_loading; }
double Backend::progress() const { return m_progress; }
QString Backend::error() const { return m_error; }
bool Backend::playing() const { return m_player.playing(); }
double Backend::playhead() const { return m_playhead; }
double Backend::cursor() const { return m_cursor; }
bool Backend::hasSelection() const { return m_hasSelection; }
double Backend::selectionStart() const { return m_selStart; }
double Backend::selectionEnd() const { return m_selEnd; }
double Backend::speed() const { return m_player.speed(); }
double Backend::volume() const { return m_player.volume(); }

bool Backend::persist() const { return m_persist; }

QObject *Backend::metronome() { return &m_metronome; }
bool Backend::sectionPending() const { return m_sectionPending; }
double Backend::sectionPendingAt() const { return m_sectionPendingAt; }
bool Backend::canUndo() const { return !m_undo.empty(); }
bool Backend::canRedo() const { return !m_redo.empty(); }

int Backend::loopMode() const {
    if (m_hasSelection)
        return 2;
    return m_cursor > kMinimumSpan ? 1 : 0;
}

double Backend::loopStart() const {
    return m_hasSelection ? m_selStart : (m_cursor > kMinimumSpan ? m_cursor : 0);
}

double Backend::loopEnd() const { return m_hasSelection ? m_selEnd : m_duration; }

QVariantList Backend::sections() const {
    QVariantList list;
    for (const Section &s : m_sections) {
        QVariantMap map;
        map.insert(QStringLiteral("start"), s.start);
        map.insert(QStringLiteral("end"), s.end);
        map.insert(QStringLiteral("name"), s.name);
        list.append(map);
    }
    return list;
}

void Backend::setError(const QString &message) {
    m_error = message;
    emit errorChanged();
}

void Backend::clearError() {
    if (m_error.isEmpty())
        return;
    m_error.clear();
    emit errorChanged();
}

void Backend::setPlayhead(double sec) {
    const double v = qBound(0.0, sec, m_duration);
    if (qFuzzyCompare(v, m_playhead))
        return;
    m_playhead = v;
    emit playheadChanged();
}

// ------------------------------------------------------------ open files ---

void Backend::open() {
    m_picker->openAudio();
}

void Backend::resetTrackState() {
    m_player.stop();
    m_player.setSource(nullptr, 0);
    m_frames.clear();
    m_peaks.clear();
    m_duration = 0;
    m_playhead = 0;
    m_cursor = 0;
    m_hasSelection = false;
    m_selStart = m_selEnd = 0;
    m_sectionPending = false;
    m_undo.clear();
    m_redo.clear();
    m_dirty = false;
    m_saveTimer.stop();
    emit sectionPendingChanged();
    emit undoChanged();
}

void Backend::openPath(const QUrl &url) {
    const QString path = url.toLocalFile().isEmpty() ? url.toString() : url.toLocalFile();
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isFile()) {
        setError(tr("File not found: %1").arg(path));
        return;
    }
    resetTrackState();
    m_filePath = info.absoluteFilePath();
    m_fileName = info.fileName();
    m_error.clear();
    m_loading = true;
    m_progress = 0;
    emit errorChanged();
    emit trackChanged();
    emit loadingChanged();
    m_decoder->load(m_filePath);
}

// -------------------------------------------------------------- playback ---

void Backend::togglePlay() {
    if (!ready())
        return;
    if (m_player.playing()) {
        // Stopping leaves the start cursor untouched; the playhead simply
        // stays where it stopped, as a bookmark the start cursor can be
        // placed at (or near) with a click or the arrow keys.
        const double stoppedAt = qBound(0.0, m_player.inputPosition(), m_duration);
        m_player.stop();
        m_tick.stop();
        setPlayhead(stoppedAt);
    } else {
        applyLoopToPlayer();
        m_player.play();
        if (m_player.playing()) {
            // With the click running, beat one lands on the play start.
            if (m_metronome.enabled())
                m_metronome.restart();
            m_tick.start();
        }
    }
}

void Backend::applyLoopToPlayer() {
    if (!ready())
        return;
    const double a = loopStart();
    const double b = qMax(a + kMinimumSpan, loopEnd());
    const bool wasOutside = m_player.playing() &&
                            (m_player.inputPosition() < a - 0.001 ||
                             m_player.inputPosition() >= b - 0.001);
    m_player.setLoop(a, b);
    if (wasOutside)
        m_player.seek(a);
    emit loopChanged();
}

void Backend::setCursor(double sec) {
    if (!ready())
        return;
    const double v = qBound(0.0, sec, m_duration);
    if (qFuzzyCompare(v, m_cursor))
        return;
    m_cursor = v;
    emit cursorChanged();
    if (!m_player.playing())
        setPlayhead(loopStart());
    applyLoopToPlayer();
    scheduleSave();
}

void Backend::select(double start, double end) {
    if (!ready())
        return;
    m_selStart = qBound(0.0, qMin(start, end), m_duration);
    m_selEnd = qBound(0.0, qMax(start, end), m_duration);
    m_hasSelection = (m_selEnd - m_selStart) > kMinimumSpan;
    emit selectionChanged();
    if (!m_player.playing())
        setPlayhead(loopStart());
    applyLoopToPlayer();
    scheduleSave();
}

void Backend::clearSelection() {
    if (!m_hasSelection)
        return;
    m_hasSelection = false;
    emit selectionChanged();
    if (!m_player.playing())
        setPlayhead(loopStart());
    applyLoopToPlayer();
    scheduleSave();
}

void Backend::seekRelative(double delta) {
    if (!ready())
        return;
    if (m_player.playing()) {
        const double base = m_hasSelection ? m_selStart : loopStart();
        m_player.seek(qBound(base, m_playhead + delta, loopEnd() - 0.01));
        setPlayhead(m_player.inputPosition());
    } else {
        // While stopped the arrows walk the start cursor in small steps,
        // from where the cursor stands — never from the playhead bookmark.
        setCursor(qBound(0.0, m_cursor + delta, m_duration));
        emit centerRequested(m_cursor);
    }
}

void Backend::setSpeed(double speed) {
    m_player.setSpeed(speed); // clamps to 0.2..1.5 and emits speedChanged
}

void Backend::setVolume(double volume) {
    m_player.setVolume(volume);
}

void Backend::nudgeSpeed(double delta) {
    setSpeed(m_player.speed() + delta);
}

void Backend::nudgeVolume(double delta) {
    m_player.setVolume(m_player.volume() + delta); // Player clamps to 0..1
}

// -------------------------------------------------------------- sections ---

double Backend::markerTime() const { return m_playhead; }

void Backend::beginSection() {
    if (!ready())
        return;
    if (!m_sectionPending) {
        m_sectionPending = true;
        m_sectionPendingAt = markerTime();
        emit sectionPendingChanged();
        return;
    }
    const double start = m_sectionPendingAt;
    const double end = markerTime();
    cancelSection();
    if (end - start > kMinimumSpan)
        requestSectionName(start, end);
    else
        setError(tr("Section is too short; press M again further apart."));
}

void Backend::sectionFromSelection() {
    if (!ready() || !m_hasSelection)
        return;
    cancelSection();
    requestSectionName(m_selStart, m_selEnd);
}

void Backend::requestSectionName(double start, double end) {
    QVariantMap request;
    request.insert(QStringLiteral("start"), start);
    request.insert(QStringLiteral("end"), end);
    request.insert(QStringLiteral("suggestion"),
                   tr("%1 – %2").arg(formatTime(start, false), formatTime(end, false)));
    m_namingRequest = request;
    emit nameSectionRequested(start, end, request.value(QStringLiteral("suggestion")).toString());
}

void Backend::confirmSection(const QString &name) {
    const QVariantMap request = m_namingRequest.toMap();
    if (request.isEmpty())
        return;
    pushUndo();
    Section section;
    section.start = request.value(QStringLiteral("start")).toDouble();
    section.end = request.value(QStringLiteral("end")).toDouble();
    section.name = name.trimmed().isEmpty() ? request.value(QStringLiteral("suggestion")).toString()
                                            : name.trimmed();
    m_sections.append(section);
    std::sort(m_sections.begin(), m_sections.end(),
              [](const Section &a, const Section &b) { return a.start < b.start; });
    m_namingRequest = {};
    m_dirty = true;
    emit sectionsChanged();
    scheduleSave();
}

void Backend::cancelSection() {
    if (m_sectionPending) {
        m_sectionPending = false;
        emit sectionPendingChanged();
    }
    m_namingRequest = {};
}

void Backend::deleteSection(int index) {
    if (index < 0 || index >= m_sections.size())
        return;
    pushUndo();
    m_sections.removeAt(index);
    m_dirty = true;
    emit sectionsChanged();
    scheduleSave();
}

void Backend::renameSection(int index, const QString &name) {
    if (index < 0 || index >= m_sections.size())
        return;
    pushUndo();
    m_sections[index].name = name.trimmed();
    m_dirty = true;
    emit sectionsChanged();
    scheduleSave();
}

void Backend::jumpToSection(int index) {
    if (index < 0 || index >= m_sections.size())
        return;
    setCursor(m_sections[index].start);
    emit centerRequested(m_sections[index].start);
}

void Backend::playSection(int index) {
    if (index < 0 || index >= m_sections.size())
        return;
    select(m_sections[index].start, m_sections[index].end);
    emit centerRequested(m_sections[index].start);
    if (!m_player.playing())
        togglePlay();
}

int Backend::sectionAtTime(double t) const {
    for (int i = 0; i < m_sections.size(); ++i)
        if (t >= m_sections[i].start && t < m_sections[i].end)
            return i;
    return -1;
}

void Backend::pushUndo() {
    m_undo.push_back(sectionsJson());
    if (m_undo.size() > kUndoDepth)
        m_undo.erase(m_undo.begin());
    m_redo.clear();
    emit undoChanged();
}

void Backend::undo() {
    if (m_undo.empty())
        return;
    m_redo.push_back(sectionsJson());
    setSectionsFromJson(m_undo.back());
    m_undo.pop_back();
    emit undoChanged();
    m_dirty = true;
    scheduleSave();
}

void Backend::redo() {
    if (m_redo.empty())
        return;
    m_undo.push_back(sectionsJson());
    setSectionsFromJson(m_redo.back());
    m_redo.pop_back();
    emit undoChanged();
    m_dirty = true;
    scheduleSave();
}

QJsonArray Backend::sectionsJson() const {
    QJsonArray array;
    for (const Section &s : m_sections) {
        QJsonObject object;
        object.insert(QStringLiteral("start"), s.start);
        object.insert(QStringLiteral("end"), s.end);
        object.insert(QStringLiteral("name"), s.name);
        array.append(object);
    }
    return array;
}

void Backend::setSectionsFromJson(const QJsonArray &array) {
    QVector<Section> sections;
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        Section s;
        s.start = object.value(QStringLiteral("start")).toDouble();
        s.end = object.value(QStringLiteral("end")).toDouble();
        s.name = object.value(QStringLiteral("name")).toString();
        if (s.end - s.start > 0.01 && s.start >= 0 && (m_duration <= 0 || s.start < m_duration))
            sections.append(s);
    }
    std::sort(sections.begin(), sections.end(),
              [](const Section &a, const Section &b) { return a.start < b.start; });
    m_sections = sections;
    emit sectionsChanged();
}

// --------------------------------------------------------------- sidecar ---

QString Backend::sidecarPath(const QString &audioPath) {
    return audioPath + QStringLiteral(".riffer.json");
}

QJsonObject Backend::readSidecar(const QString &audioPath) {
    QFile file(sidecarPath(audioPath));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

bool Backend::writeSidecar(const QString &audioPath, const QJsonObject &object) {
    QSaveFile file(sidecarPath(audioPath)); // written beside, renamed atomically
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    return file.commit();
}

void Backend::loadSidecarState() {
    const QJsonObject saved = readSidecar(m_filePath);
    if (saved.isEmpty())
        return;
    if (saved.contains(QStringLiteral("speed")))
        m_player.setSpeed(saved.value(QStringLiteral("speed")).toDouble());
    if (saved.contains(QStringLiteral("volume")))
        m_player.setVolume(saved.value(QStringLiteral("volume")).toDouble());
    const QJsonObject selection = saved.value(QStringLiteral("selection")).toObject();
    if (selection.contains(QStringLiteral("start")) && selection.contains(QStringLiteral("end"))) {
        m_selStart = qBound(0.0, selection.value(QStringLiteral("start")).toDouble(), m_duration);
        m_selEnd = qBound(0.0, selection.value(QStringLiteral("end")).toDouble(), m_duration);
        m_hasSelection = (m_selEnd - m_selStart) > kMinimumSpan;
        emit selectionChanged();
    }
    setSectionsFromJson(saved.value(QStringLiteral("sections")).toArray());
    // Persistence is opt-in, but a sidecar written by an earlier opt-in
    // session keeps this track saving until it is switched off again.
    m_persist = saved.value(QStringLiteral("autosave")).toBool();
    emit persistChanged();
}

void Backend::scheduleSave() {
    if (!m_persist)
        return; // nothing is written unless this track opted in
    m_dirty = true;
    m_saveTimer.start();
}

void Backend::setPersist(bool persist) {
    if (m_persist == persist)
        return;
    m_persist = persist;
    emit persistChanged();
    if (persist) {
        m_dirty = true;
        saveNow(); // write the sidecar immediately, no debounce
    } else if (!m_filePath.isEmpty()) {
        QFile::remove(sidecarPath(m_filePath)); // opting out declutters
        m_dirty = false;
    }
}

void Backend::saveNow() {
    if (m_filePath.isEmpty() || !m_dirty)
        return;
    QJsonObject object;
    object.insert(QStringLiteral("version"), 1);
    object.insert(QStringLiteral("autosave"), true);
    object.insert(QStringLiteral("speed"), m_player.speed());
    object.insert(QStringLiteral("volume"), m_player.volume());
    if (m_hasSelection) {
        QJsonObject selection;
        selection.insert(QStringLiteral("start"), m_selStart);
        selection.insert(QStringLiteral("end"), m_selEnd);
        object.insert(QStringLiteral("selection"), selection);
    }
    object.insert(QStringLiteral("sections"), sectionsJson());
    if (!writeSidecar(m_filePath, object))
        setError(tr("Could not save alongside %1.").arg(m_fileName));
    m_dirty = false;
}

// ---------------------------------------------------------------- format ---

QString Backend::formatTime(double sec, bool tenths) const {
    if (sec < 0)
        sec = 0;
    const qint64 t = qint64(qRound(sec * 10.0)); // tenths
    const qint64 h = t / 36000;
    const int m = int((t / 600) % 60);
    const int s = int((t / 10) % 60);
    const int d = int(t % 10);
    const QLocale loc;
    const auto two = [&](int v) {
        return v < 10 ? QStringLiteral("0") + loc.toString(v) : loc.toString(v);
    };
    QString out = h > 0 ? loc.toString(h) + QStringLiteral(":") + two(m) + QStringLiteral(":")
                           + two(s)
                       : loc.toString(m) + QStringLiteral(":") + two(s);
    if (tenths)
        out += QString(loc.decimalPoint()) + loc.toString(d);
    return out;
}

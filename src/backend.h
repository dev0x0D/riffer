#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVector>
#include <vector>

#include "decoder.h"
#include "peaks.h"
#include "player.h"

class FilePicker;

// One audio file and its practice state: speed, volume, loop selection and
// named sections, persisted to a sidecar beside the audio. The cursor is a
// per-session thing and is not persisted, so a track always opens at its
// start. Owns the decode pipeline and the playback engine, and is the single
// context property QML talks to.
class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString fileName READ fileName NOTIFY trackChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY trackChanged)
    Q_PROPERTY(double duration READ duration NOTIFY trackChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY trackChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(double progress READ progress NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(double playhead READ playhead NOTIFY playheadChanged)
    Q_PROPERTY(double cursor READ cursor WRITE setCursor NOTIFY cursorChanged)
    Q_PROPERTY(bool hasSelection READ hasSelection NOTIFY selectionChanged)
    Q_PROPERTY(double selectionStart READ selectionStart NOTIFY selectionChanged)
    Q_PROPERTY(double selectionEnd READ selectionEnd NOTIFY selectionChanged)
    // 0 = whole track, 1 = from the cursor, 2 = the selection.
    Q_PROPERTY(int loopMode READ loopMode NOTIFY loopChanged)
    Q_PROPERTY(double loopStart READ loopStart NOTIFY loopChanged)
    Q_PROPERTY(double loopEnd READ loopEnd NOTIFY loopChanged)
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(QVariantList sections READ sections NOTIFY sectionsChanged)
    Q_PROPERTY(bool sectionPending READ sectionPending NOTIFY sectionPendingChanged)
    Q_PROPERTY(double sectionPendingAt READ sectionPendingAt NOTIFY sectionPendingChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoChanged)
    // Opt-in sidecar persistence: off by default, so a track does not grow
    // a .riffer.json unless it was asked for on this track.
    Q_PROPERTY(bool persist READ persist WRITE setPersist NOTIFY persistChanged)

public:
    struct Section {
        double start = 0;
        double end = 0;
        QString name;
    };

    // decoder and picker are injectable for tests; both default to the real ones.
    explicit Backend(Decoder *decoder = nullptr, FilePicker *picker = nullptr,
                     QObject *parent = nullptr);
    ~Backend() override;

    QString fileName() const;
    QString filePath() const;
    double duration() const;
    bool ready() const;
    bool loading() const;
    double progress() const;
    QString error() const;
    bool playing() const;
    double playhead() const;
    double cursor() const;
    bool hasSelection() const;
    double selectionStart() const;
    double selectionEnd() const;
    int loopMode() const;
    double loopStart() const;
    double loopEnd() const;
    double speed() const;
    double volume() const;
    QVariantList sections() const;
    bool sectionPending() const;
    double sectionPendingAt() const;
    bool canUndo() const;
    bool canRedo() const;
    bool persist() const;

public slots:
    void setCursor(double sec);
    void setSpeed(double speed);
    void setVolume(double volume);

    Q_INVOKABLE void open();                     // through the portal picker
    Q_INVOKABLE void openPath(const QUrl &url);
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void nudgeSpeed(double delta);
    Q_INVOKABLE void nudgeVolume(double delta);  // arrow up/down, ±delta
    Q_INVOKABLE void seekRelative(double delta); // arrows; moves the start cursor while stopped
    void setPersist(bool persist);
    Q_INVOKABLE void select(double start, double end);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void beginSection();             // M: start, M again: end
    Q_INVOKABLE void sectionFromSelection();     // Shift+M
    Q_INVOKABLE void confirmSection(const QString &name);
    Q_INVOKABLE void cancelSection();
    Q_INVOKABLE void deleteSection(int index);
    Q_INVOKABLE void renameSection(int index, const QString &name);
    Q_INVOKABLE void jumpToSection(int index);
    Q_INVOKABLE void playSection(int index);
    Q_INVOKABLE int sectionAtTime(double t) const;
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE QString formatTime(double sec, bool tenths) const;

    // --- C++ side for the waveform item ---
    const std::vector<float> &frames() const { return m_frames; }
    const std::vector<PeakLevel> &peaks() const { return m_peaks; }
    const QVector<Section> &sectionList() const { return m_sections; }

    // --- sidecar, kept beside the audio file and written atomically ---
    static QString sidecarPath(const QString &audioPath);
    static QJsonObject readSidecar(const QString &audioPath);
    static bool writeSidecar(const QString &audioPath, const QJsonObject &object);

signals:
    void trackChanged();
    void loadingChanged();
    void errorChanged();
    void playingChanged();
    void playheadChanged();
    void cursorChanged();
    void selectionChanged();
    void loopChanged();
    void speedChanged();
    void volumeChanged();
    void sectionsChanged();
    void sectionPendingChanged();
    void undoChanged();
    void persistChanged();
    void nameSectionRequested(double start, double end, const QString &suggestion);
    void centerRequested(double sec); // QML scrolls the waveform view

private:
    void setError(const QString &message);
    void setPlayhead(double sec);
    void applyLoopToPlayer();
    void pushUndo();
    QJsonArray sectionsJson() const;
    void setSectionsFromJson(const QJsonArray &array);
    void loadSidecarState();
    void saveNow();
    void scheduleSave();
    double markerTime() const;
    void resetTrackState();
    void requestSectionName(double start, double end);

    Decoder *m_decoder = nullptr; // owned when injected as nullptr
    FilePicker *m_picker = nullptr;
    Player m_player;
    std::vector<float> m_frames;
    std::vector<PeakLevel> m_peaks;
    QString m_filePath;
    QString m_fileName;
    QString m_error;
    double m_duration = 0;
    bool m_loading = false;
    double m_progress = 0;
    double m_playhead = 0;
    double m_cursor = 0;
    bool m_hasSelection = false;
    double m_selStart = 0;
    double m_selEnd = 0;
    QVector<Section> m_sections;
    bool m_sectionPending = false;
    double m_sectionPendingAt = 0;
    QVariant m_namingRequest; // {start, end, suggestion} while the dialog is up
    std::vector<QJsonArray> m_undo;
    std::vector<QJsonArray> m_redo;
    QTimer m_saveTimer; // debounced sidecar autosave
    QTimer m_tick;      // playhead refresh while playing
    bool m_dirty = false;
    bool m_persist = false; // opt-in sidecar writing, off until asked for
};

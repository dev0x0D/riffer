#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "clock.h"
#include "decoder.h"
#include "peaks.h"
#include "theme.h"
#include "translator.h"

class RifferTests : public QObject {
    Q_OBJECT

private slots:
    // ---------------------------------------------------------------- clock
    void clockMapsTempoAndWraps() {
        PlayClock clock;
        clock.reset(0, 1.0);
        QCOMPARE(clock.inputAt(48000), 48000.0);

        clock.setTempo(96000, 0.5); // half speed from here on
        QCOMPARE(clock.inputAt(96000), 96000.0);
        QCOMPARE(clock.inputAt(192000), 96000.0 + 96000 * 0.5);

        clock.wrapped(240000, 0); // loop restarts at the very beginning
        QCOMPARE(clock.inputAt(240000), 0.0);
        QCOMPARE(clock.inputAt(240480), 240.0); // still at half speed
    }

    void clockReplacesPointsOnTheSameFrame() {
        PlayClock clock;
        clock.reset(0, 1.0);
        clock.setTempo(1000, 0.7);
        clock.setTempo(1000, 0.6); // a slider drag: the later change wins
        QCOMPARE(clock.inputAt(2000), 1000.0 + 1000 * 0.6);
    }

    void clockRejectsTimeTravel() {
        PlayClock clock;
        clock.reset(0, 1.0);
        clock.setTempo(5000, 0.5);
        clock.wrapped(2000, 100); // late record after a newer one
        QCOMPARE(clock.inputAt(6000), 5000.0 + 1000 * 0.5);
    }

    // ---------------------------------------------------------------- peaks
    void peaksBuildPyramid() {
        // 700 frames: bucket 0 covers 0..255, bucket 1 covers 256..511…
        std::vector<float> frames(700 * 2, 0.1f);
        frames[300 * 2] = 1.0f;
        frames[300 * 2 + 1] = 1.0f;
        frames[650 * 2] = -1.0f;
        frames[650 * 2 + 1] = -1.0f;

        const std::vector<PeakLevel> levels = buildPeaks(frames.data(), 700, 2);
        QCOMPARE(qint64(levels.size()), 2); // 3 buckets -> one more level
        QCOMPARE(qint64(levels[0].bucketFrames), 256);
        QCOMPARE(qint64(levels[0].peaks.size()), 3);
        QCOMPARE(levels[0].peaks[1].maxV, 1.0f);   // frame 300
        QCOMPARE(levels[0].peaks[2].minV, -1.0f);  // frame 650
        QCOMPARE(levels[1].peaks[0].maxV, 1.0f);
        QCOMPARE(levels[1].peaks[0].minV, -1.0f);
    }

    void peaksCombineAndPickLevel() {
        std::vector<float> frames(1000 * 2, 0.0f);
        const std::vector<PeakLevel> levels = buildPeaks(frames.data(), 1000, 2);
        QCOMPARE(levelForBucket(levels, 1), -1); // below the pyramid: read raw
        QCOMPARE(levelForBucket(levels, 256), 0);
        QCOMPARE(levelForBucket(levels, 1000000), qint64(levels.size()) > 1 ? 1 : 0);

        const Peak combined = combinePeaks(levels[0], 0, 3);
        QCOMPARE(combined.minV, 0.0f);
        QCOMPARE(combined.maxV, 0.0f);

        const Peak clamped = combinePeaks(levels[0], 2, 100); // past the end
        QCOMPARE(qint64(levels[0].peaks.size()), 4);
        QCOMPARE(clamped.minV, levels[0].peaks[3].minV);
    }

    // -------------------------------------------------------------- decoder
    void decoderDecodesThroughFakeFfmpeg() {
        QTemporaryDir dir;
        // One second of stereo 48 kHz, amplitude ramping up.
        QByteArray raw;
        for (int i = 0; i < 48000; ++i) {
            const float v = float(i) / 48000.0f;
            raw.append(reinterpret_cast<const char *>(&v), 4);
            raw.append(reinterpret_cast<const char *>(&v), 4);
        }
        const QString samples = dir.filePath("samples.f32");
        writeFile(samples, raw);

        const QString ffmpeg = dir.filePath("ffmpeg");
        writeFile(ffmpeg, "#!/bin/sh\ncat '" + samples.toUtf8() + "'\n");
        QFile::setPermissions(ffmpeg, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const QString ffprobe = dir.filePath("ffprobe");
        writeFile(ffprobe, "#!/bin/sh\necho 1.0\n");
        QFile::setPermissions(ffprobe, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

        Decoder decoder(ffmpeg, ffprobe);
        QSignalSpy decoded(&decoder, &Decoder::decoded);
        QSignalSpy failed(&decoder, &Decoder::failed);
        decoder.load(dir.filePath("track.wav"));
        QTRY_COMPARE_WITH_TIMEOUT(decoded.count(), 1, 10000);
        QCOMPARE(failed.count(), 0);

        const std::vector<float> frames = decoder.takeFrames();
        QCOMPARE(qint64(frames.size()), 48000 * 2);
        QCOMPARE(decoded.first().at(1).toDouble(), 1.0); // seconds
        QVERIFY(decoder.takeFrames().empty());           // ownership moved out
    }

    // -------------------------------------------------------------- backend
    void backendLoadsDecodesAndSavesSidecar() {
        QTemporaryDir dir;
        const QString samples = dir.filePath("samples.f32");
        QByteArray raw;
        for (int i = 0; i < 48000; ++i) {
            const float v = float(i) / 48000.0f;
            raw.append(reinterpret_cast<const char *>(&v), 4);
            raw.append(reinterpret_cast<const char *>(&v), 4);
        }
        writeFile(samples, raw);
        const QString ffmpeg = dir.filePath("ffmpeg");
        writeFile(ffmpeg, "#!/bin/sh\ncat '" + samples.toUtf8() + "'\n");
        QFile::setPermissions(ffmpeg, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const QString ffprobe = dir.filePath("ffprobe");
        writeFile(ffprobe, "#!/bin/sh\necho 1.0\n");
        QFile::setPermissions(ffprobe, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);

        const QString track = dir.filePath("song.mp3");
        writeFile(track, "not really an mp3, the fake ffmpeg ignores it");

        {
            Decoder *decoder = new Decoder(ffmpeg, ffprobe);
            Backend backend(decoder);
            backend.openPath(QUrl::fromLocalFile(track));
            QTRY_VERIFY_WITH_TIMEOUT(backend.ready(), 10000);
            QCOMPARE(backend.duration(), 1.0);
            QCOMPARE(backend.loopMode(), 0);
            QCOMPARE(backend.loopStart(), 0.0);

            backend.select(0.1, 0.6);
            QCOMPARE(backend.loopMode(), 2);
            QCOMPARE(backend.loopStart(), 0.1);
            QCOMPARE(backend.loopEnd(), 0.6);
            backend.setSpeed(4.0);
            QCOMPARE(backend.speed(), 1.5); // clamped
            backend.setSpeed(0.75);

            // Section from the selection, through the naming request.
            QSignalSpy naming(&backend, &Backend::nameSectionRequested);
            backend.sectionFromSelection();
            QCOMPARE(naming.count(), 1);
            backend.confirmSection("Chorus");
            QCOMPARE(backend.sections().size(), 1);
            QCOMPARE(backend.sections().first().toMap().value("name").toString(),
                     QString("Chorus"));
            QCOMPARE(backend.sectionAtTime(0.2), 0);
            QCOMPARE(backend.sectionAtTime(0.95), -1);

            // Persistence is opt-in; enabling it writes the sidecar at once.
            backend.setPersist(true);
            QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(Backend::sidecarPath(track)), 4000);
        }

        // A fresh backend restores the saved practice state, and section
        // edits are undoable across it.
        {
            Decoder *decoder = new Decoder(ffmpeg, ffprobe);
            Backend backend(decoder);
            backend.openPath(QUrl::fromLocalFile(track));
            QTRY_VERIFY_WITH_TIMEOUT(backend.ready(), 10000);
            QCOMPARE(backend.speed(), 0.75);
            QVERIFY(backend.hasSelection());
            QCOMPARE(backend.selectionStart(), 0.1);
            QCOMPARE(backend.sections().size(), 1);
            QCOMPARE(backend.sections().first().toMap().value("name").toString(),
                     QString("Chorus"));
            QVERIFY(!backend.canUndo());

            backend.renameSection(0, "Solo");
            QCOMPARE(backend.sections().first().toMap().value("name").toString(),
                     QString("Solo"));
            QVERIFY(backend.canUndo());
            backend.undo();
            QCOMPARE(backend.sections().first().toMap().value("name").toString(),
                     QString("Chorus"));
            backend.redo();
            QCOMPARE(backend.sections().first().toMap().value("name").toString(),
                     QString("Solo"));

            backend.deleteSection(0);
            QCOMPARE(backend.sections().size(), 0);
            backend.undo();
            QCOMPARE(backend.sections().size(), 1);
            backend.deleteSection(0);
            QVERIFY(!backend.canRedo()); // a new edit drops the redo stack

            // The opt-in flag rode along in the sidecar, and opting out
            // removes the file again.
            QCOMPARE(backend.persist(), true);
            backend.setPersist(false);
            QVERIFY(!QFile::exists(Backend::sidecarPath(track)));
        }
    }

    void backendSidecarStatics() {
        QTemporaryDir dir;
        const QString audio = dir.filePath("song.wav");
        QCOMPARE(Backend::sidecarPath(audio), audio + QString(".riffer.json"));
        QJsonObject saved;
        saved.insert("speed", 0.5);
        saved.insert("sections", QJsonArray{QJsonObject{{"start", 0.0}, {"end", 1.0}, {"name", "A"}}});
        QVERIFY(Backend::writeSidecar(audio, saved));
        const QJsonObject loaded = Backend::readSidecar(audio);
        QCOMPARE(loaded.value("speed").toDouble(), 0.5);
        QCOMPARE(loaded.value("sections").toArray().size(), 1);
        QFile::remove(Backend::sidecarPath(audio));
        QVERIFY(Backend::readSidecar(audio).isEmpty());
    }

    void backendLoopContextAndClamps() {
        Decoder *decoder = new Decoder;
        Backend backend(decoder);
        // No track: nothing may crash and nothing may claim readiness.
        backend.togglePlay();
        backend.select(1, 2);
        backend.setCursor(3);
        QCOMPARE(backend.ready(), false);
        QCOMPARE(backend.loopMode(), 0);

        backend.setSpeed(0.01);
        QCOMPARE(backend.speed(), 0.2);
        backend.setSpeed(9.9);
        QCOMPARE(backend.speed(), 1.5);
        backend.setSpeed(0.25);
        backend.nudgeSpeed(-1.0);
        QCOMPARE(backend.speed(), 0.2);
    }

    void backendFormatTime() {
        Decoder *decoder = new Decoder;
        Backend backend(decoder);
        const QLocale locale;
        QCOMPARE(backend.formatTime(0, false), QString("0:00"));
        QCOMPARE(backend.formatTime(61, false), QString("1:01"));
        QCOMPARE(backend.formatTime(83.4, true), QString("1:23") + locale.decimalPoint() + QString("4"));
        QCOMPARE(backend.formatTime(3725, false), QString("1:02:05"));
        QCOMPARE(backend.formatTime(-5, false), QString("0:00"));
    }

    // ---------------------------------------------------------------- theme
    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent() {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }

    void themeFollowsHyprlandRounding() {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        writeFile(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        writeFile(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        writeFile(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    // ----------------------------------------------------------- translator
    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile() {
        QTemporaryDir dir;
        writeFile(dir.path() + "/language", "pt-BR\n");
        writeFile(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        writeFile(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("pt-BR"));
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms() {
        QTemporaryDir dir;
        writeFile(dir.path() + "/language", "ru\n");
        writeFile(dir.path() + "/en.json",
                  R"({"%n sections": {"one": "%n section", "other": "%n sections"}})");
        writeFile(dir.path() + "/ru.json",
                  R"({"%n sections": {"one": "%n секция", "few": "%n секции", "many": "%n секций", "other": "%n секции"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n sections", nullptr, 21), QString("%n секция"));
        QCOMPARE(translator.translate("Main", "%n sections", nullptr, 22), QString("%n секции"));
        QCOMPARE(translator.translate("Main", "%n sections", nullptr, 25), QString("%n секций"));
        QCOMPARE(Translator::pluralCategory("ar", 11), QString("many"));
        QCOMPARE(Translator::pluralCategory("ja", 1), QString("other"));
    }

private:
    static void writeFile(const QString &path, const QByteArray &contents) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }
};

QTEST_MAIN(RifferTests)
#include "riffer_tests.moc"

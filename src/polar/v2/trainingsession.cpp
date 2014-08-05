/*
    Copyright 2014 Paul Colby

    This file is part of Bipolar.

    Bipolar is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Biplar is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Bipolar.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "trainingsession.h"

#include "message.h"
#include "types.h"

#include "os/versioninfo.h"

#include <QApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_WIN
#include <QtZlib/zlib.h>
#else
#include <zlib.h>
#endif

// These constants match those used by Polar's V2 API.
#define AUTOLAPS   QLatin1String("autolaps")
#define CREATE     QLatin1String("create")
#define LAPS       QLatin1String("laps")
#define ROUTE      QLatin1String("route")
#define RRSAMPLES  QLatin1String("rrsamples")
#define SAMPLES    QLatin1String("samples")
#define STATISTICS QLatin1String("statistics")
#define ZONES      QLatin1String("zones")

namespace polar {
namespace v2 {

TrainingSession::TrainingSession(const QString &baseName)
    : baseName(baseName)
{

}

/// @see https://github.com/pcolby/bipolar/wiki/Polar-Sport-Types
QString TrainingSession::getTcxSport(const quint64 &polarSportValue)
{
    #define TCX_RUNNING QLatin1String("Running")
    #define TCX_BIKING  QLatin1String("Biking")
    #define TCX_OTHER   QLatin1String("Other")
    static QMap<quint64, QString> map;
    if (map.isEmpty()) {
        map.insert( 1, TCX_RUNNING); // Running
        map.insert( 2, TCX_BIKING);  // Cycling
        map.insert( 3, TCX_OTHER);   // Walking
        map.insert( 4, TCX_OTHER);   // Jogging
        map.insert( 5, TCX_BIKING);  // Mountain biking
        map.insert( 6, TCX_OTHER);   // Skiing
        map.insert( 7, TCX_OTHER);   // Downhill skiing
        map.insert( 8, TCX_OTHER);   // Rowing
        map.insert( 9, TCX_OTHER);   // Nordic walking
        map.insert(10, TCX_OTHER);   // Skating
        map.insert(11, TCX_OTHER);   // Hiking
        map.insert(12, TCX_OTHER);   // Tennis
        map.insert(13, TCX_OTHER);   // Squash
        map.insert(14, TCX_OTHER);   // Badminton
        map.insert(15, TCX_OTHER);   // Strength training
        map.insert(16, TCX_OTHER);   // Other outdoor
        map.insert(17, TCX_RUNNING); // Treadmill running
        map.insert(18, TCX_BIKING);  // Indoor cycling
        map.insert(19, TCX_RUNNING); // Road running
        map.insert(20, TCX_OTHER);   // Circuit training
        map.insert(22, TCX_OTHER);   // Snowboarding
        map.insert(23, TCX_OTHER);   // Swimming
        map.insert(24, TCX_OTHER);   // Freestyle XC skiing
        map.insert(25, TCX_OTHER);   // Classic XC skiing
        map.insert(27, TCX_RUNNING); // Trail running
        map.insert(28, TCX_OTHER);   // Ice skating
        map.insert(29, TCX_OTHER);   // Inline skating
        map.insert(30, TCX_OTHER);   // Roller skating
        map.insert(32, TCX_OTHER);   // Group exercise
        map.insert(33, TCX_OTHER);   // Yoga
        map.insert(34, TCX_OTHER);   // Crossfit
        map.insert(35, TCX_OTHER);   // Golf
        map.insert(36, TCX_RUNNING); // Track&field running
        map.insert(38, TCX_BIKING);  // Road biking
        map.insert(39, TCX_OTHER);   // Soccer
        map.insert(40, TCX_OTHER);   // Cricket
        map.insert(41, TCX_OTHER);   // Basketball
        map.insert(42, TCX_OTHER);   // Baseball
        map.insert(43, TCX_OTHER);   // Rugby
        map.insert(44, TCX_OTHER);   // Field hockey
        map.insert(45, TCX_OTHER);   // Volleyball
        map.insert(46, TCX_OTHER);   // Ice hockey
        map.insert(47, TCX_OTHER);   // Football
        map.insert(48, TCX_OTHER);   // Handball
        map.insert(49, TCX_OTHER);   // Beach volley
        map.insert(50, TCX_OTHER);   // Futsal
        map.insert(51, TCX_OTHER);   // Floorball
        map.insert(52, TCX_OTHER);   // Dancing
        map.insert(53, TCX_OTHER);   // Trotting
        map.insert(54, TCX_OTHER);   // Riding
        map.insert(55, TCX_OTHER);   // Cross-trainer
        map.insert(56, TCX_OTHER);   // Fitness martial arts
        map.insert(57, TCX_OTHER);   // Functional training
        map.insert(58, TCX_OTHER);   // Bootcamp
        map.insert(59, TCX_OTHER);   // Freestyle roller skiing
        map.insert(60, TCX_OTHER);   // Classic roller skiing
        map.insert(61, TCX_OTHER);   // Aerobics
        map.insert(62, TCX_OTHER);   // Aqua fitness
        map.insert(63, TCX_OTHER);   // Step workout
        map.insert(64, TCX_OTHER);   // Body&amp;Mind
        map.insert(65, TCX_OTHER);   // Pilates
        map.insert(66, TCX_OTHER);   // Stretching
        map.insert(67, TCX_OTHER);   // Fitness dancing
        map.insert(68, TCX_OTHER);   // Triathlon
        map.insert(69, TCX_OTHER);   // Duathlon
        map.insert(70, TCX_OTHER);   // Off-road triathlon
        map.insert(71, TCX_OTHER);   // Off-road duathlon
        map.insert(82, TCX_OTHER);   // Multisport
        map.insert(83, TCX_OTHER);   // Other indoor
    }
    QMap<quint64, QString>::ConstIterator iter = map.constFind(polarSportValue);
    if (iter == map.constEnd()) {
        qWarning() << "unknown polar sport value" << polarSportValue;
    }
    return (iter == map.constEnd()) ? TCX_OTHER : iter.value();
}

bool TrainingSession::isGzipped(const QByteArray &data)
{
    return data.startsWith("\x1f\x8b");
}

bool TrainingSession::isGzipped(QIODevice &data)
{
    return isGzipped(data.peek(2));
}

bool TrainingSession::isValid() const
{
    return !parsedExercises.isEmpty();
}

bool TrainingSession::parse(const QString &baseName)
{
    parsedExercises.clear();

    if (!baseName.isEmpty()) {
        this->baseName = baseName;
    }

    if (this->baseName.isEmpty()) {
        Q_ASSERT(!baseName.isEmpty());
        qWarning() << "parse called with no baseName specified";
        return false;
    }

    parsedPhysicalInformation = parsePhysicalInformation(
        this->baseName + QLatin1String("-physical-information"));

    parsedSession = parseCreateSession(this->baseName + QLatin1String("-create"));

    QMap<QString, QMap<QString, QString> > fileNames;
    const QFileInfo fileInfo(this->baseName);
    foreach (const QFileInfo &entryInfo, fileInfo.dir().entryInfoList(
             QStringList(fileInfo.fileName() + QLatin1String("-*"))))
    {
        const QStringList nameParts = entryInfo.fileName().split(QLatin1Char('-'));
        if ((nameParts.size() >= 3) && (nameParts.at(nameParts.size() - 3) == QLatin1String("exercises"))) {
            fileNames[nameParts.at(nameParts.size() - 2)][nameParts.at(nameParts.size() - 1)] = entryInfo.filePath();
        }
    }

    for (QMap<QString, QMap<QString, QString> >::const_iterator iter = fileNames.constBegin();
         iter != fileNames.constEnd(); ++iter)
    {
        parse(iter.key(), iter.value());
    }

    return isValid();
}

bool TrainingSession::parse(const QString &exerciseId, const QMap<QString, QString> &fileNames)
{
    QVariantMap exercise;
    QVariantList sources;
    #define PARSE_IF_CONTAINS(str, Func) \
        if (fileNames.contains(str)) { \
            const QVariantMap map = parse##Func(fileNames.value(str)); \
            if (!map.empty()) { \
                exercise[str] = map; \
                sources << fileNames.value(str); \
            } \
        }
    PARSE_IF_CONTAINS(AUTOLAPS,   Laps);
    PARSE_IF_CONTAINS(CREATE,     CreateExercise);
    PARSE_IF_CONTAINS(LAPS,       Laps);
  //PARSE_IF_CONTAINS(PHASES,     Phases);
    PARSE_IF_CONTAINS(ROUTE,      Route);
    PARSE_IF_CONTAINS(RRSAMPLES,  RRSamples);
    PARSE_IF_CONTAINS(SAMPLES,    Samples);
  //PARSE_IF_CONTAINS(SENSORS,    Sensors);
    PARSE_IF_CONTAINS(STATISTICS, Statistics);
    PARSE_IF_CONTAINS(ZONES,      Zones);
    #undef PARSE_IF_CONTAINS

    if (!exercise.empty()) {
        exercise[QLatin1String("sources")] = sources;
        parsedExercises[exerciseId] = exercise;
        return true;
    }
    return false;
}

#define ADD_FIELD_INFO(tag, name, type) \
    fieldInfo[QLatin1String(tag)] = ProtoBuf::Message::FieldInfo( \
        QLatin1String(name), ProtoBuf::Types::type \
    )

QVariantMap TrainingSession::parseCreateExercise(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1",     "start",         EmbeddedMessage);
    ADD_FIELD_INFO("1/1",   "date",          EmbeddedMessage);
    ADD_FIELD_INFO("1/1/1", "year",          Uint32);
    ADD_FIELD_INFO("1/1/2", "month",         Uint32);
    ADD_FIELD_INFO("1/1/3", "day",           Uint32);
    ADD_FIELD_INFO("1/2",   "time",          EmbeddedMessage);
    ADD_FIELD_INFO("1/2/1", "hour",          Uint32);
    ADD_FIELD_INFO("1/2/2", "minute",        Uint32);
    ADD_FIELD_INFO("1/2/3", "seconds",       Uint32);
    ADD_FIELD_INFO("1/2/4", "milliseconds",  Uint32);
    ADD_FIELD_INFO("1/4",   "offset",        Int32);
    ADD_FIELD_INFO("2",     "duration",      EmbeddedMessage);
    ADD_FIELD_INFO("2/1",   "hours",         Uint32);
    ADD_FIELD_INFO("2/2",   "minutes",       Uint32);
    ADD_FIELD_INFO("2/3",   "seconds",       Uint32);
    ADD_FIELD_INFO("2/4",   "milliseconds",  Uint32);
    ADD_FIELD_INFO("3",     "sport",         EmbeddedMessage);
    ADD_FIELD_INFO("3/1",   "value",         Uint64);
    ADD_FIELD_INFO("4",     "distance",      Float);
    ADD_FIELD_INFO("5",     "calories",      Uint32);
    ADD_FIELD_INFO("6",     "training-load", EmbeddedMessage);
    ADD_FIELD_INFO("6/1",   "load-value",    Uint32);
    ADD_FIELD_INFO("6/2",   "recovery-time", EmbeddedMessage);
    ADD_FIELD_INFO("6/2/1", "hours",         Uint32);
    ADD_FIELD_INFO("6/2/2", "minutes",       Uint32);
    ADD_FIELD_INFO("6/2/3", "seconds",       Uint32);
    ADD_FIELD_INFO("6/2/4", "milliseconds",  Uint32);
    ADD_FIELD_INFO("6/3",   "carbs",         Uint32);
    ADD_FIELD_INFO("6/4",   "protein",       Uint32);
    ADD_FIELD_INFO("6/5",   "fat",           Uint32);
    ADD_FIELD_INFO("7",     "sensors",       Enumerator);
    ADD_FIELD_INFO("9",     "running-index", EmbeddedMessage);
    ADD_FIELD_INFO("9/1",   "value",         Uint32);
    ADD_FIELD_INFO("9/2",   "duration",      EmbeddedMessage);
    ADD_FIELD_INFO("9/2/1", "hours",         Uint32);
    ADD_FIELD_INFO("9/2/2", "minutes",       Uint32);
    ADD_FIELD_INFO("9/2/3", "seconds",       Uint32);
    ADD_FIELD_INFO("9/2/4", "milliseconds",  Uint32);
    ADD_FIELD_INFO("10",    "ascent",        Float);
    ADD_FIELD_INFO("11",    "descent",       Float);
    ADD_FIELD_INFO("12",    "latitude",      Double);
    ADD_FIELD_INFO("13",    "longitude",     Double);
    ADD_FIELD_INFO("14",    "place",         String);
    ADD_FIELD_INFO("15",    "exercise-result",          EmbeddedMessage); /// @todo
    ADD_FIELD_INFO("16",    "exercise-counters",        EmbeddedMessage); /// @todo
    ADD_FIELD_INFO("17",    "speed-calibration-offset", Float);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parseCreateExercise(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open exercise-create file" << fileName;
        return QVariantMap();
    }
    return parseCreateExercise(file);
}

QVariantMap TrainingSession::parseCreateSession(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1",      "start",              EmbeddedMessage);
    ADD_FIELD_INFO("1/1",    "date",               EmbeddedMessage);
    ADD_FIELD_INFO("1/1/1",  "year",               Uint32);
    ADD_FIELD_INFO("1/1/2",  "month",              Uint32);
    ADD_FIELD_INFO("1/1/3",  "day",                Uint32);
    ADD_FIELD_INFO("1/2",    "time",               EmbeddedMessage);
    ADD_FIELD_INFO("1/2/1",  "hour",               Uint32);
    ADD_FIELD_INFO("1/2/2",  "minute",             Uint32);
    ADD_FIELD_INFO("1/2/3",  "seconds",            Uint32);
    ADD_FIELD_INFO("1/2/4",  "milliseconds",       Uint32);
    ADD_FIELD_INFO("1/4",    "offset",             Int32);
    ADD_FIELD_INFO("2",      "exercise-count",     Uint32);
    ADD_FIELD_INFO("3",      "device",             String);
    ADD_FIELD_INFO("4",      "model",              String);
    ADD_FIELD_INFO("5",      "duration",           EmbeddedMessage);
    ADD_FIELD_INFO("5/1",    "hours",              Uint32);
    ADD_FIELD_INFO("5/2",    "minutes",            Uint32);
    ADD_FIELD_INFO("5/3",    "seconds",            Uint32);
    ADD_FIELD_INFO("5/4",    "milliseconds",       Uint32);
    ADD_FIELD_INFO("6",      "distance",           Float);
    ADD_FIELD_INFO("7",      "calories",           Uint32);
    ADD_FIELD_INFO("8",      "heartreat",          EmbeddedMessage);
    ADD_FIELD_INFO("8/1",    "average",            Uint32);
    ADD_FIELD_INFO("8/2",    "maximum",            Uint32);
    ADD_FIELD_INFO("9",      "heartrate-duration", EmbeddedMessage);
    ADD_FIELD_INFO("9/1",    "hours",              Uint32);
    ADD_FIELD_INFO("9/2",    "minutes",            Uint32);
    ADD_FIELD_INFO("9/3",    "seconds",            Uint32);
    ADD_FIELD_INFO("9/4",    "milliseconds",       Uint32);
    ADD_FIELD_INFO("10",     "training-load",      EmbeddedMessage);
    ADD_FIELD_INFO("10/1",   "load-value",         Uint32);
    ADD_FIELD_INFO("10/2",   "recovery-time",      EmbeddedMessage);
    ADD_FIELD_INFO("10/2/1", "hours",              Uint32);
    ADD_FIELD_INFO("10/2/2", "minutes",            Uint32);
    ADD_FIELD_INFO("10/2/3", "seconds",            Uint32);
    ADD_FIELD_INFO("10/2/4", "milliseconds",       Uint32);
    ADD_FIELD_INFO("10/3",   "carbs",              Uint32);
    ADD_FIELD_INFO("10/4",   "protein",            Uint32);
    ADD_FIELD_INFO("10/5",   "fat",                Uint32);
    ADD_FIELD_INFO("11",     "session-name",       EmbeddedMessage);
    ADD_FIELD_INFO("11/1",   "text",               String);
    ADD_FIELD_INFO("12",     "feeling",            Float);
    ADD_FIELD_INFO("13",     "note",               EmbeddedMessage);
    ADD_FIELD_INFO("13/1",   "text",               String);
    ADD_FIELD_INFO("14",     "place",              EmbeddedMessage);
    ADD_FIELD_INFO("14/1",   "text",               String);
    ADD_FIELD_INFO("15",     "latitude",           Double);
    ADD_FIELD_INFO("16",     "longitude",          Double);
    ADD_FIELD_INFO("17",     "benefit",            Enumerator);
    ADD_FIELD_INFO("18",     "sport",              EmbeddedMessage);
    ADD_FIELD_INFO("18/1",   "value",              Uint64);
    ADD_FIELD_INFO("19",     "training-target",    EmbeddedMessage);
    ADD_FIELD_INFO("19/1",   "value",              Uint64);
    ADD_FIELD_INFO("19/2",   "last-modified",      EmbeddedMessage);
    ADD_FIELD_INFO("19/2/1",   "date",             EmbeddedMessage);
    ADD_FIELD_INFO("19/2/1/1", "year",             Uint32);
    ADD_FIELD_INFO("19/2/1/2", "month",            Uint32);
    ADD_FIELD_INFO("19/2/1/3", "day",              Uint32);
    ADD_FIELD_INFO("19/2/2",   "time",             EmbeddedMessage);
    ADD_FIELD_INFO("19/2/2/1", "hour",             Uint32);
    ADD_FIELD_INFO("19/2/2/2", "minute",           Uint32);
    ADD_FIELD_INFO("19/2/2/3", "seconds",          Uint32);
    ADD_FIELD_INFO("19/2/2/4", "milliseconds",     Uint32);
    ADD_FIELD_INFO("20",     "end",                EmbeddedMessage);
    ADD_FIELD_INFO("20/1",   "date",               EmbeddedMessage);
    ADD_FIELD_INFO("20/1/1", "year",               Uint32);
    ADD_FIELD_INFO("20/1/2", "month",              Uint32);
    ADD_FIELD_INFO("20/1/3", "day",                Uint32);
    ADD_FIELD_INFO("20/2",   "time",               EmbeddedMessage);
    ADD_FIELD_INFO("20/2/1", "hour",               Uint32);
    ADD_FIELD_INFO("20/2/2", "minute",             Uint32);
    ADD_FIELD_INFO("20/2/3", "seconds",            Uint32);
    ADD_FIELD_INFO("20/2/4", "milliseconds",       Uint32);
    ADD_FIELD_INFO("20/4",   "offset",             Int32);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parseCreateSession(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open session-create file" << fileName;
        return QVariantMap();
    }
    return parseCreateSession(file);
}

QVariantMap TrainingSession::parseLaps(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1",        "laps",             EmbeddedMessage);
    ADD_FIELD_INFO("1/1",      "header",           EmbeddedMessage);
    ADD_FIELD_INFO("1/1/1",    "split-time",       EmbeddedMessage);
    ADD_FIELD_INFO("1/1/1/1",  "hours",            Uint32);
    ADD_FIELD_INFO("1/1/1/2",  "minutes",          Uint32);
    ADD_FIELD_INFO("1/1/1/3",  "seconds",          Uint32);
    ADD_FIELD_INFO("1/1/1/4",  "milliseconds",     Uint32);
    ADD_FIELD_INFO("1/1/2",    "duration",         EmbeddedMessage);
    ADD_FIELD_INFO("1/1/2/1",  "hours",            Uint32);
    ADD_FIELD_INFO("1/1/2/2",  "minutes",          Uint32);
    ADD_FIELD_INFO("1/1/2/3",  "seconds",          Uint32);
    ADD_FIELD_INFO("1/1/2/4",  "milliseconds",     Uint32);
    ADD_FIELD_INFO("1/1/3",    "distance",         Float);
    ADD_FIELD_INFO("1/1/4",    "ascent",           Float);
    ADD_FIELD_INFO("1/1/5",    "descent",          Float);
    ADD_FIELD_INFO("1/1/6",    "lap-type",         Enumerator);
    ADD_FIELD_INFO("1/2",      "stats",            EmbeddedMessage);
    ADD_FIELD_INFO("1/2/1",    "heartrate",        EmbeddedMessage);
    ADD_FIELD_INFO("1/2/1/1",  "average",          Uint32);
    ADD_FIELD_INFO("1/2/1/2",  "maximum",          Uint32);
    ADD_FIELD_INFO("1/2/1/3",  "minimum",          Uint32);
    ADD_FIELD_INFO("1/2/2",    "speed",            EmbeddedMessage);
    ADD_FIELD_INFO("1/2/2/1",  "average",          Float);
    ADD_FIELD_INFO("1/2/2/2",  "maximum",          Float);
    ADD_FIELD_INFO("1/2/3",    "cadence",          EmbeddedMessage);
    ADD_FIELD_INFO("1/2/3/1",  "average",          Uint32);
    ADD_FIELD_INFO("1/2/3/2",  "maximum",          Uint32);
    ADD_FIELD_INFO("1/2/4",    "power",            EmbeddedMessage);
    ADD_FIELD_INFO("1/2/4/1",  "average",          Uint32);
    ADD_FIELD_INFO("1/2/4/2",  "maximum",          Uint32);
    ADD_FIELD_INFO("1/2/5",    "pedaling",         EmbeddedMessage);
    ADD_FIELD_INFO("1/2/5/1",  "average",          Uint32);
    ADD_FIELD_INFO("1/2/6",    "incline",          EmbeddedMessage);
    ADD_FIELD_INFO("1/2/6/1",  "average",          Float);
    ADD_FIELD_INFO("1/2/7",    "stride",           EmbeddedMessage);
    ADD_FIELD_INFO("1/2/7",    "average",          Uint32);
    ADD_FIELD_INFO("2",        "summary",          EmbeddedMessage);
    ADD_FIELD_INFO("2/1",      "best-duration",    EmbeddedMessage);
    ADD_FIELD_INFO("2/1/1",    "hours",            Uint32);
    ADD_FIELD_INFO("2/1/2",    "minutes",          Uint32);
    ADD_FIELD_INFO("2/1/3",    "seconds",          Uint32);
    ADD_FIELD_INFO("2/1.4",    "milliseconds",     Uint32);
    ADD_FIELD_INFO("2/2",      "average-duration", EmbeddedMessage);
    ADD_FIELD_INFO("2/2/1",    "hours",            Uint32);
    ADD_FIELD_INFO("2/2/2",    "minutes",          Uint32);
    ADD_FIELD_INFO("2/2/3",    "seconds",          Uint32);
    ADD_FIELD_INFO("2/2.4",    "milliseconds",     Uint32);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parseLaps(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open laps file" << fileName;
        return QVariantMap();
    }
    return parseLaps(file);
}

QVariantMap TrainingSession::parsePhysicalInformation(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1",        "birthday",            EmbeddedMessage);
    ADD_FIELD_INFO("1/1",      "value",               EmbeddedMessage);
    ADD_FIELD_INFO("1/1/1",    "year",                Uint32);
    ADD_FIELD_INFO("1/1/2",    "month",               Uint32);
    ADD_FIELD_INFO("1/1/3",    "day",                 Uint32);
    ADD_FIELD_INFO("1/2",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("1/2/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("1/2/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("1/2/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("1/2/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("1/2/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("1/2/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("1/2/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("1/2/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("1/2/2/4",  "milliseconds",        Uint32);
    ADD_FIELD_INFO("2",        "gender",              EmbeddedMessage);
    ADD_FIELD_INFO("2/1",      "value",               Enumerator);
    ADD_FIELD_INFO("2/2",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("2/2/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("2/2/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("2/2/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("2/2/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("2/2/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("2/2/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("2/2/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("2/2/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("2/2/2/4",  "milliseconds",        Uint32);
    ADD_FIELD_INFO("3",        "weight",              EmbeddedMessage);
    ADD_FIELD_INFO("3/1",      "value",               Float);
    ADD_FIELD_INFO("3/2",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("3/2/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("3/2/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("3/2/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("3/2/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("3/2/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("3/2/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("3/2/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("3/2/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("3/2/2/4",  "milliseconds",        Uint32);
    ADD_FIELD_INFO("4",        "height",              EmbeddedMessage);
    ADD_FIELD_INFO("4/1",      "value",               Float);
    ADD_FIELD_INFO("4/2",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("4/2/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("4/2/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("4/2/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("4/2/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("4/2/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("4/2/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("4/2/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("4/2/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("4/2/2/4",  "milliseconds",        Uint32);
    ADD_FIELD_INFO("5",        "maximum-heartrate",   EmbeddedMessage);
    ADD_FIELD_INFO("5/1",      "value",               Uint32);
    ADD_FIELD_INFO("5/2",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("5/2/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("5/2/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("5/2/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("5/2/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("5/2/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("5/2/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("5/2/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("5/2/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("5/2/2/4",  "milliseconds",        Uint32);
    ADD_FIELD_INFO("5/3",      "source",              Enumerator);
    ADD_FIELD_INFO("6",        "resting-heartrate",   EmbeddedMessage);
    ADD_FIELD_INFO("6/1",      "value",               Uint32);
    ADD_FIELD_INFO("6/2",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("6/2/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("6/2/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("6/2/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("6/2/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("6/2/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("6/2/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("6/2/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("6/2/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("6/2/2/4",  "milliseconds",        Uint32);
    ADD_FIELD_INFO("6/3",      "source",              Enumerator);
    ADD_FIELD_INFO("8",        "aerobic-threshold",   EmbeddedMessage);
    ADD_FIELD_INFO("8/1",      "value",               Uint32);
    ADD_FIELD_INFO("8/2",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("8/2/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("8/2/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("8/2/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("8/2/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("8/2/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("8/2/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("8/2/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("8/2/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("8/2/2/4",  "milliseconds",        Uint32);
    ADD_FIELD_INFO("8/3",      "source",              Enumerator);
    ADD_FIELD_INFO("9",        "anaerobic-threshold", EmbeddedMessage);
    ADD_FIELD_INFO("9/1",      "value",               Uint32);
    ADD_FIELD_INFO("9/2",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("9/2/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("9/2/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("9/2/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("9/2/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("9/2/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("9/2/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("9/2/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("9/2/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("9/2/2/4",  "milliseconds",        Uint32);
    ADD_FIELD_INFO("9/3",      "source",              Enumerator);
    ADD_FIELD_INFO("10",       "vo2max",              EmbeddedMessage);
    ADD_FIELD_INFO("10/1",     "value",               Uint32);
    ADD_FIELD_INFO("10/2",     "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("10/2/1",   "date",                EmbeddedMessage);
    ADD_FIELD_INFO("10/2/1/1", "year",                Uint32);
    ADD_FIELD_INFO("10/2/1/2", "month",               Uint32);
    ADD_FIELD_INFO("10/2/1/3", "day",                 Uint32);
    ADD_FIELD_INFO("10/2/2",   "time",                EmbeddedMessage);
    ADD_FIELD_INFO("10/2/2/1", "hour",                Uint32);
    ADD_FIELD_INFO("10/2/2/2", "minute",              Uint32);
    ADD_FIELD_INFO("10/2/2/3", "seconds",             Uint32);
    ADD_FIELD_INFO("10/2/2/4", "milliseconds",        Uint32);
    ADD_FIELD_INFO("10/3",     "source",              Enumerator);
    ADD_FIELD_INFO("11",       "training-background", EmbeddedMessage);
    ADD_FIELD_INFO("11/1",     "value",               Enumerator);
    ADD_FIELD_INFO("11/2",     "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("11/2/1",   "date",                EmbeddedMessage);
    ADD_FIELD_INFO("11/2/1/1", "year",                Uint32);
    ADD_FIELD_INFO("11/2/1/2", "month",               Uint32);
    ADD_FIELD_INFO("11/2/1/3", "day",                 Uint32);
    ADD_FIELD_INFO("11/2/2",   "time",                EmbeddedMessage);
    ADD_FIELD_INFO("11/2/2/1", "hour",                Uint32);
    ADD_FIELD_INFO("11/2/2/2", "minute",              Uint32);
    ADD_FIELD_INFO("11/2/2/3", "seconds",             Uint32);
    ADD_FIELD_INFO("11/2/2/4", "milliseconds",        Uint32);
    ADD_FIELD_INFO("13",       "13",                  EmbeddedMessage); // ???
    ADD_FIELD_INFO("13/1",     "13/1",                Float);           // ???
    ADD_FIELD_INFO("13/2",     "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("13/2/1",   "date",                EmbeddedMessage);
    ADD_FIELD_INFO("13/2/1/1", "year",                Uint32);
    ADD_FIELD_INFO("13/2/1/2", "month",               Uint32);
    ADD_FIELD_INFO("13/2/1/3", "day",                 Uint32);
    ADD_FIELD_INFO("13/2/2",   "time",                EmbeddedMessage);
    ADD_FIELD_INFO("13/2/2/1", "hour",                Uint32);
    ADD_FIELD_INFO("13/2/2/2", "minute",              Uint32);
    ADD_FIELD_INFO("13/2/2/3", "seconds",             Uint32);
    ADD_FIELD_INFO("13/2/2/4", "milliseconds",        Uint32);
    ADD_FIELD_INFO("100",      "modified",            EmbeddedMessage);
    ADD_FIELD_INFO("100/1",    "date",                EmbeddedMessage);
    ADD_FIELD_INFO("100/1/1",  "year",                Uint32);
    ADD_FIELD_INFO("100/1/2",  "month",               Uint32);
    ADD_FIELD_INFO("100/1/3",  "day",                 Uint32);
    ADD_FIELD_INFO("100/2",    "time",                EmbeddedMessage);
    ADD_FIELD_INFO("100/2/1",  "hour",                Uint32);
    ADD_FIELD_INFO("100/2/2",  "minute",              Uint32);
    ADD_FIELD_INFO("100/2/3",  "seconds",             Uint32);
    ADD_FIELD_INFO("100/2/4",  "milliseconds",        Uint32);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parsePhysicalInformation(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open physical information file" << fileName;
        return QVariantMap();
    }
    return parsePhysicalInformation(file);
}

QVariantMap TrainingSession::parseRoute(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1",     "duration",     Uint32);
    ADD_FIELD_INFO("2",     "latitude",     Double);
    ADD_FIELD_INFO("3",     "longitude",    Double);
    ADD_FIELD_INFO("4",     "altitude",     Sint32);
    ADD_FIELD_INFO("5",     "satellites",   Uint32);
    ADD_FIELD_INFO("9",     "timestamp",    EmbeddedMessage);
    ADD_FIELD_INFO("9/1",   "date",         EmbeddedMessage);
    ADD_FIELD_INFO("9/1/1", "year",         Uint32);
    ADD_FIELD_INFO("9/1/2", "month",        Uint32);
    ADD_FIELD_INFO("9/1/3", "day",          Uint32);
    ADD_FIELD_INFO("9/2",   "time",         EmbeddedMessage);
    ADD_FIELD_INFO("9/2/1", "hour",         Uint32);
    ADD_FIELD_INFO("9/2/2", "minute",       Uint32);
    ADD_FIELD_INFO("9/2/3", "seconds",      Uint32);
    ADD_FIELD_INFO("9/2/4", "milliseconds", Uint32);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parseRoute(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open route file" << fileName;
        return QVariantMap();
    }
    return parseRoute(file);
}

QVariantMap TrainingSession::parseRRSamples(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1", "value", Uint32);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parseRRSamples(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open rrsamples file" << fileName;
        return QVariantMap();
    }
    return parseRRSamples(file);
}

QVariantMap TrainingSession::parseSamples(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1",     "record-interval",          EmbeddedMessage);
    ADD_FIELD_INFO("1/1",   "hours",                    Uint32);
    ADD_FIELD_INFO("1/2",   "minutes",                  Uint32);
    ADD_FIELD_INFO("1/3",   "seconds",                  Uint32);
    ADD_FIELD_INFO("1/4",   "milliseconds",             Uint32);
    ADD_FIELD_INFO("2",     "heartrate",                Uint32);
    ADD_FIELD_INFO("3",     "heartrate-offline",        EmbeddedMessage);
    ADD_FIELD_INFO("3/1",   "start-index",              Uint32);
    ADD_FIELD_INFO("3/2",   "stop-index",               Uint32);
    ADD_FIELD_INFO("4",     "cadence",                  Uint32);
    ADD_FIELD_INFO("5",     "cadence-offline",          EmbeddedMessage);
    ADD_FIELD_INFO("5/1",   "start-index",              Uint32);
    ADD_FIELD_INFO("5/2",   "stop-index",               Uint32);
    ADD_FIELD_INFO("6",     "altitude",                 Float);
    ADD_FIELD_INFO("7",     "altitude-calibration",     EmbeddedMessage);
    ADD_FIELD_INFO("7/1",   "start-index",              Uint32);
    ADD_FIELD_INFO("7/2",   "value",                    Float);
    ADD_FIELD_INFO("7/3",   "operation",                Enumerator);
    ADD_FIELD_INFO("7/4",   "cause",                    Enumerator);
    ADD_FIELD_INFO("8",     "temperature",              Float);
    ADD_FIELD_INFO("9",     "speed",                    Float);
    ADD_FIELD_INFO("10",    "speed-offline",            EmbeddedMessage);
    ADD_FIELD_INFO("10/1",  "start-index",              Uint32);
    ADD_FIELD_INFO("10/2",  "stop-index",               Uint32);
    ADD_FIELD_INFO("11",    "distance",                 Float);
    ADD_FIELD_INFO("12",    "distance-offline",         EmbeddedMessage);
    ADD_FIELD_INFO("12/1",  "start-index",              Uint32);
    ADD_FIELD_INFO("12/2",  "stop-index",               Uint32);
    ADD_FIELD_INFO("13",    "stride-length",            Uint32);
    ADD_FIELD_INFO("14",    "stride-offline",           EmbeddedMessage);
    ADD_FIELD_INFO("14/1",  "start-index",              Uint32);
    ADD_FIELD_INFO("14/2",  "stop-index",               Uint32);
    ADD_FIELD_INFO("15",    "stride-calibration",       EmbeddedMessage);
    ADD_FIELD_INFO("15/1",  "start-index",              Uint32);
    ADD_FIELD_INFO("15/2",  "value",                    Float);
    ADD_FIELD_INFO("15/3",  "operation",                Enumerator);
    ADD_FIELD_INFO("15/4",  "cause",                    Enumerator);
    ADD_FIELD_INFO("16",    "fwd-acceleration",         Float);
    ADD_FIELD_INFO("17",    "moving-type",              Enumerator);
    ADD_FIELD_INFO("18",    "altitude-offline",         EmbeddedMessage);
    ADD_FIELD_INFO("18/1",  "start-index",              Uint32);
    ADD_FIELD_INFO("18/2",  "stop-index",               Uint32);
    ADD_FIELD_INFO("19",    "temperature-offline",      EmbeddedMessage);
    ADD_FIELD_INFO("19/1",  "start-index",              Uint32);
    ADD_FIELD_INFO("19/2",  "stop-index",               Uint32);
    ADD_FIELD_INFO("20",    "fwd-acceleration-offline", EmbeddedMessage);
    ADD_FIELD_INFO("20/1",  "start-index",              Uint32);
    ADD_FIELD_INFO("20/2",  "stop-index",               Uint32);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parseSamples(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open samples file" << fileName;
        return QVariantMap();
    }
    return parseSamples(file);
}

QVariantMap TrainingSession::parseStatistics(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1",    "heartrate",      EmbeddedMessage);
    ADD_FIELD_INFO("1/1",  "minimum",        Uint32);
    ADD_FIELD_INFO("1/2",  "average",        Uint32);
    ADD_FIELD_INFO("1/3",  "maximum",        Uint32);
    ADD_FIELD_INFO("2",    "speed",          EmbeddedMessage);
    ADD_FIELD_INFO("2/1",  "average",        Float);
    ADD_FIELD_INFO("2/2",  "maximum",        Float);
    ADD_FIELD_INFO("3",    "cadence",        EmbeddedMessage);
    ADD_FIELD_INFO("3/1",  "average",        Uint32);
    ADD_FIELD_INFO("3/1",  "maximum",        Uint32);
    ADD_FIELD_INFO("4",    "altitude",       EmbeddedMessage);
    ADD_FIELD_INFO("4/1",  "minimum",        Float);
    ADD_FIELD_INFO("4/2",  "average",        Float);
    ADD_FIELD_INFO("4/3",  "maximum",        Float);
    ADD_FIELD_INFO("5",    "power",          EmbeddedMessage);
    ADD_FIELD_INFO("5/1",  "average",        Uint32);
    ADD_FIELD_INFO("5/1",  "maximum",        Uint32);
    ADD_FIELD_INFO("6",    "lr_balance",     EmbeddedMessage);
    ADD_FIELD_INFO("6/1",  "average",        Float);
    ADD_FIELD_INFO("7",    "temperature",    EmbeddedMessage);
    ADD_FIELD_INFO("7/1",  "minimum",        Float);
    ADD_FIELD_INFO("7/2",  "average",        Float);
    ADD_FIELD_INFO("7/3",  "maximum",        Float);
    ADD_FIELD_INFO("8",    "activity",       EmbeddedMessage);
    ADD_FIELD_INFO("8/1",  "average",        Float);
    ADD_FIELD_INFO("9",    "stride",         EmbeddedMessage);
    ADD_FIELD_INFO("9/1",  "average",        Uint32);
    ADD_FIELD_INFO("9/1",  "maximum",        Uint32);
    ADD_FIELD_INFO("10",   "include",        EmbeddedMessage);
    ADD_FIELD_INFO("10/1", "average",        Float);
    ADD_FIELD_INFO("10/1", "maximum",        Float);
    ADD_FIELD_INFO("11",   "declince",       EmbeddedMessage);
    ADD_FIELD_INFO("11/1", "average",        Float);
    ADD_FIELD_INFO("11/1", "maximum",        Float);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parseStatistics(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open stats file" << fileName;
        return QVariantMap();
    }
    return parseStatistics(file);
}

QVariantMap TrainingSession::parseZones(QIODevice &data) const
{
    ProtoBuf::Message::FieldInfoMap fieldInfo;
    ADD_FIELD_INFO("1",     "heartrate",        EmbeddedMessage);
    ADD_FIELD_INFO("1/1",   "limits",           EmbeddedMessage);
    ADD_FIELD_INFO("1/1/1", "low",              Uint32);
    ADD_FIELD_INFO("1/1/2", "high",             Uint32);
    ADD_FIELD_INFO("1/2",   "duration",         EmbeddedMessage);
    ADD_FIELD_INFO("1/2/1", "hours",            Uint32);
    ADD_FIELD_INFO("1/2/2", "minutes",          Uint32);
    ADD_FIELD_INFO("1/2/3", "seconds",          Uint32);
    ADD_FIELD_INFO("1/2.4", "milliseconds",     Uint32);
    ADD_FIELD_INFO("2",     "power",            EmbeddedMessage);
    ADD_FIELD_INFO("2/1",   "limits",           EmbeddedMessage);
    ADD_FIELD_INFO("2/1/1", "low",              Uint32);
    ADD_FIELD_INFO("2/1/2", "high",             Uint32);
    ADD_FIELD_INFO("2/2",   "duration",         EmbeddedMessage);
    ADD_FIELD_INFO("2/2/1", "hours",            Uint32);
    ADD_FIELD_INFO("2/2/2", "minutes",          Uint32);
    ADD_FIELD_INFO("2/2/3", "seconds",          Uint32);
    ADD_FIELD_INFO("2/2.4", "milliseconds",     Uint32);
    ADD_FIELD_INFO("3",     "fatfit",           EmbeddedMessage);
    ADD_FIELD_INFO("3/1",   "limit",            Uint32);
    ADD_FIELD_INFO("3/2",   "fit-duration",     EmbeddedMessage);
    ADD_FIELD_INFO("3/2/1", "hours",            Uint32);
    ADD_FIELD_INFO("3/2/2", "minutes",          Uint32);
    ADD_FIELD_INFO("3/2/3", "seconds",          Uint32);
    ADD_FIELD_INFO("3/2.4", "milliseconds",     Uint32);
    ADD_FIELD_INFO("3/3",   "fat-duration",     EmbeddedMessage);
    ADD_FIELD_INFO("3/3/1", "hours",            Uint32);
    ADD_FIELD_INFO("3/3/2", "minutes",          Uint32);
    ADD_FIELD_INFO("3/3/3", "seconds",          Uint32);
    ADD_FIELD_INFO("3/3.4", "milliseconds",     Uint32);
    ADD_FIELD_INFO("4",     "speed",            EmbeddedMessage);
    ADD_FIELD_INFO("4/1",   "limits",           EmbeddedMessage);
    ADD_FIELD_INFO("4/1/1", "low",              Float);
    ADD_FIELD_INFO("4/1/2", "high",             Float);
    ADD_FIELD_INFO("4/2",   "duration",         EmbeddedMessage);
    ADD_FIELD_INFO("4/2/1", "hours",            Uint32);
    ADD_FIELD_INFO("4/2/2", "minutes",          Uint32);
    ADD_FIELD_INFO("4/2/3", "seconds",          Uint32);
    ADD_FIELD_INFO("4/2.4", "milliseconds",     Uint32);
    ADD_FIELD_INFO("4/3",   "distance",         Float);
    ADD_FIELD_INFO("10",    "heartrate-source", Enumerator);
    ProtoBuf::Message parser(fieldInfo);

    if (isGzipped(data)) {
        QByteArray array = unzip(data.readAll());
        return parser.parse(array);
    } else {
        return parser.parse(data);
    }
}

QVariantMap TrainingSession::parseZones(const QString &fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "failed to open zones file" << fileName;
        return QVariantMap();
    }
    return parseZones(file);
}

/**
 * @brief Fetch the first item from a list contained within a QVariant.
 *
 * This is just a convenience function that prevents us from having to perform
 * the basic QList::isEmpty() check in many, many places.
 *
 * @param variant QVariant (probably) containing a list.
 *
 * @return The first item in the list, or an invalid variant if there is no
 *         such list, or the list is empty.
 */
QVariant first(const QVariant &variant) {
    const QVariantList list = variant.toList();
    return (list.isEmpty()) ? QVariant() : list.first();
}

QVariantMap firstMap(const QVariant &list) {
    return first(list).toMap();
}

QDateTime getDateTime(const QVariantMap &map)
{
    const QVariantMap date = firstMap(map.value(QLatin1String("date")));
    const QVariantMap time = firstMap(map.value(QLatin1String("time")));
    const QString string = QString::fromLatin1("%1-%2-%3 %4:%5:%6.%7")
        .arg(first(date.value(QLatin1String("year"))).toString())
        .arg(first(date.value(QLatin1String("month"))).toString())
        .arg(first(date.value(QLatin1String("day"))).toString())
        .arg(first(time.value(QLatin1String("hour"))).toString())
        .arg(first(time.value(QLatin1String("minute"))).toString())
        .arg(first(time.value(QLatin1String("seconds"))).toString())
        .arg(first(time.value(QLatin1String("milliseconds"))).toString());
    QDateTime dateTime = QDateTime::fromString(string, QLatin1String("yyyy-M-d H:m:s.z"));

    const QVariantMap::const_iterator offset = map.constFind(QLatin1String("offset"));
    if (offset == map.constEnd()) {
        dateTime.setTimeSpec(Qt::UTC);
    } else {
        #if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
        dateTime.setOffsetFromUtc(first(offset.value()).toInt() * 60);
        #else /// @todo Remove this when Qt 5.2+ is available on Travis CI.
        dateTime.setUtcOffset(first(offset.value()).toInt() * 60);
        #endif
    }
    return dateTime;
}

quint64 getDuration(const QVariantMap &map)
{
    const QVariantMap::const_iterator
        hours        = map.constFind(QLatin1String("hours")),
        minutes      = map.constFind(QLatin1String("minutes")),
        seconds      = map.constFind(QLatin1String("seconds")),
        milliseconds = map.constFind(QLatin1String("milliseconds"));
    return
       ((((  hours == map.constEnd()) ? 0 : first(hours.value()).toULongLong()) * 60
       + ((minutes == map.constEnd()) ? 0 : first(minutes.value()).toULongLong())) * 60
       + ((seconds == map.constEnd()) ? 0 : first(seconds.value()).toULongLong())) * 1000
       + ((milliseconds == map.constEnd()) ? 0 : first(milliseconds.value()).toULongLong());
}

QString getDurationString(const QVariantMap &map)
{
    const QVariantMap::const_iterator
        hours        = map.constFind(QLatin1String("hours")),
        minutes      = map.constFind(QLatin1String("minutes")),
        seconds      = map.constFind(QLatin1String("seconds")),
        milliseconds = map.constFind(QLatin1String("milliseconds"));
    return QString::fromLatin1("%1:%2:%3.%4")
            .arg(first(  hours.value()).toUInt(), 2, 10, QLatin1Char('0'))
            .arg(first(minutes.value()).toUInt(), 2, 10, QLatin1Char('0'))
            .arg(first(seconds.value()).toUInt(), 2, 10, QLatin1Char('0'))
            .arg(first(milliseconds.value()).toUInt(), 3, 10, QLatin1Char('0'));
}

QString getFileName(const QString &file)
{
    const QFileInfo info(file);
    return info.fileName();
}

bool sensorOffline(const QVariantList &list, const int index)
{
    foreach (const QVariant &entry, list) {
        const QVariantMap map = entry.toMap();
        const QVariant startIndex = first(map.value(QLatin1String("start-index")));
        const QVariant endIndex = first(map.value(QLatin1String("start-index")));
        if ((!startIndex.canConvert(QMetaType::Int)) ||
            (!endIndex.canConvert(QMetaType::Int))) {
            qWarning() << "ignoring invalid 'offline' entry" << entry;
            continue;
        }
        if ((startIndex.toInt() <= index) && (index <= endIndex.toInt())) {
            return true; // Sensor was offline.
        }
    }
    return false; // Sensor was not offline.
}

bool haveAnySamples(const QVariantMap &samples, const QString &type)
{
    const int size = samples.value(type).toList().length();
    for (int index = 0; index < size; ++index) {
        if (!sensorOffline(samples.value(type + QLatin1String("-offline")).toList(), index)) {
            return true;
        }
    }
    return false;
}

/// @see http://www.topografix.com/GPX/1/1/gpx.xsd
QDomDocument TrainingSession::toGPX(const QDateTime &creationTime) const
{
    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction(QLatin1String("xml"),
        QLatin1String("version='1.0' encoding='utf-8'")));

    QDomElement gpx = doc.createElement(QLatin1String("gpx"));
    gpx.setAttribute(QLatin1String("version"), QLatin1String("1.1"));
    gpx.setAttribute(QLatin1String("creator"), QString::fromLatin1("%1 %2 - %3")
                     .arg(QApplication::applicationName())
                     .arg(QApplication::applicationVersion())
                     .arg(QLatin1String("https://github.com/pcolby/bipolar")));
    gpx.setAttribute(QLatin1String("xmlns"),
                     QLatin1String("http://www.topografix.com/GPX/1/1"));
    gpx.setAttribute(QLatin1String("xmlns:xsi"),
                     QLatin1String("http://www.w3.org/2001/XMLSchema-instance"));
    gpx.setAttribute(QLatin1String("xsi:schemaLocation"),
                     QLatin1String("http://www.topografix.com/GPX/1/1 "
                                   "http://www.topografix.com/GPX/1/1/gpx.xsd"));
    doc.appendChild(gpx);

    QDomElement metaData = doc.createElement(QLatin1String("metadata"));
    metaData.appendChild(doc.createElement(QLatin1String("name")))
        .appendChild(doc.createTextNode(getFileName(baseName)));
    metaData.appendChild(doc.createElement(QLatin1String("desc")))
        .appendChild(doc.createTextNode(tr("GPX encoding of %1")
                                        .arg(getFileName(baseName))));
    QDomElement link = doc.createElement(QLatin1String("link"));
    link.setAttribute(QLatin1String("href"), QLatin1String("https://github.com/pcolby/bipolar"));
    metaData.appendChild(doc.createElement(QLatin1String("author")))
        .appendChild(link).appendChild(doc.createElement(QLatin1String("text")))
            .appendChild(doc.createTextNode(QLatin1String("Bipolar")));
    metaData.appendChild(doc.createElement(QLatin1String("time")))
        .appendChild(doc.createTextNode(creationTime.toString(Qt::ISODate)));
    gpx.appendChild(metaData);

    foreach (const QVariant &exercise, parsedExercises) {
        const QVariantMap map = exercise.toMap();

        QDomElement trk = doc.createElement(QLatin1String("trk"));
        gpx.appendChild(trk);

        QStringList sources;
        foreach (const QVariant &source, map.value(QLatin1String("sources")).toList()) {
            sources << getFileName(source.toString());
        }
        trk.appendChild(doc.createElement(QLatin1String("src")))
            .appendChild(doc.createTextNode(sources.join(QLatin1Char(' '))));

        if (map.contains(ROUTE)) {
            const QVariantMap route = map.value(ROUTE).toMap();
            QDomElement trkseg = doc.createElement(QLatin1String("trkseg"));
            trk.appendChild(trkseg);

            // Get the starting time.
            const QDateTime startTime = getDateTime(firstMap(
                route.value(QLatin1String("timestamp"))));

            // Get the number of samples.
            const QVariantList altitude   = route.value(QLatin1String("altitude")).toList();
            const QVariantList duration   = route.value(QLatin1String("duration")).toList();
            const QVariantList latitude   = route.value(QLatin1String("latitude")).toList();
            const QVariantList longitude  = route.value(QLatin1String("longitude")).toList();
            const QVariantList satellites = route.value(QLatin1String("satellites")).toList();
            if ((duration.size() != altitude.size())  ||
                (duration.size() != latitude.size())  ||
                (duration.size() != longitude.size()) ||
                (duration.size() != satellites.size())) {
                qWarning() << "lists not all equal sizes:" << duration.size()
                           << altitude.size() << latitude.size()
                           << longitude.size() << satellites.size();
            }

            /// @todo Use lap data to split the <trk> into multiple <trkseg>?

            for (int index = 0; index < duration.size(); ++index) {
                QDomElement trkpt = doc.createElement(QLatin1String("trkpt"));
                trkpt.setAttribute(QLatin1String("lat"), latitude.at(index).toDouble());
                trkpt.setAttribute(QLatin1String("lon"), longitude.at(index).toDouble());
                /// @todo Use the barometric altitude instead, if present?
                trkpt.appendChild(doc.createElement(QLatin1String("ele")))
                    .appendChild(doc.createTextNode(altitude.at(index).toString()));
                trkpt.appendChild(doc.createElement(QLatin1String("time")))
                    .appendChild(doc.createTextNode(startTime.addMSecs(
                        duration.at(index).toLongLong()).toString(Qt::ISODate)));
                trkpt.appendChild(doc.createElement(QLatin1String("sat")))
                    .appendChild(doc.createTextNode(satellites.at(index).toString()));
                trkseg.appendChild(trkpt);
            }
        }
    }
    return doc;
}

/// @see http://www.polar.com/files/Polar_HRM_file%20format.pdf
QStringList TrainingSession::toHRM()
{
    QStringList hrmList;

    foreach (const QVariant &exercise, parsedExercises) {
        const QVariantMap map = exercise.toMap();
        const QVariantMap autoLaps   = map.value(AUTOLAPS).toMap();
        const QVariantMap create     = map.value(CREATE).toMap();
        const QVariantMap manualLaps = map.value(LAPS).toMap();
        const QVariantMap samples    = map.value(SAMPLES).toMap();
        const QVariantMap stats      = map.value(STATISTICS).toMap();
        const QVariantMap zones      = map.value(ZONES).toMap();

        const bool haveAltitude = haveAnySamples(samples, QLatin1String("speed"));
        const bool haveCadence  = haveAnySamples(samples, QLatin1String("cadence"));
        const bool haveSpeed    = haveAnySamples(samples, QLatin1String("altitude"));

        QString hrmData;
        QTextStream stream(&hrmData);

        // [Params]
        stream <<
            "[Params]\r\n"
            "Version=107\r\n"
            "Monitor=0\r\n"
            "SMode=";
        stream << (haveSpeed    ? '0' : '1'); // a) Speed
        stream << (haveCadence  ? '0' : '1'); // b) Cadence
        stream << (haveAltitude ? '0' : '1'); // c) Altitude
        stream <<
            "0" // d) Power (not supported by V800 yet).
            "0" // e) Power Left Right Ballance (not supported by V800 yet).
            "0" // f) Power Pedalling Index (not supported by V800 yet).
            "0" // g) HR/CC data (available only with Polar XTrainer Plus).
            "0" // h) US / Euro unit (always metric).
            "0" // i) Air pressure (not available).
            "\r\n";

        const QDateTime startTime = getDateTime(firstMap(create.value(QLatin1String("start"))));
        const quint64 recordInterval = getDuration(firstMap(samples.value(QLatin1String("record-interval"))));
        stream << "Date="      << startTime.toString(QLatin1String("yyyyMMdd")) << "\r\n";
        stream << "StartTime=" << startTime.toString(QLatin1String("HH:mm:ss.zzz")) << "\r\n";
        stream << "Length="    << getDurationString(firstMap(create.value(QLatin1String("duration")))) << "\r\n";
        stream << "Interval="  << qRound(recordInterval/1000.0f) << "\r\n";

        /// @todo {Upper,Lower}{1,2,3} - Need to parse *-phases file(s).
        QVariantList hrZones = zones.value(QLatin1String("heartrate")).toList();
        // Since HRM v1.4 only supports 3 target zones, try to reduce the
        // list down to three by removing zones with 0 duration.
        for (int index = 0; (hrZones.length() > 3) && (index < hrZones.length()); ++index) {
            const quint64 duration = getDuration(firstMap(hrZones.at(index)
                .toMap().value(QLatin1String("duration"))));
            if (duration == 0) {
                hrZones.removeAt(index--);
            }
        }
        for (int index = 0; (index < 3) && (index < hrZones.length()); ++index) {
            const QVariantMap limits = firstMap(hrZones.at(hrZones.length() - 3 + index).toMap().value(QLatin1String("limits")));
            stream << "Upper" << (index+1) << "=" << first(limits.value(QLatin1String("high"))).toUInt() << "\r\n";
            stream << "Lower" << (index+1) << "=" << first(limits.value(QLatin1String("low"))).toUInt() << "\r\n";
        }
        for (int index = 0; (index < 3) && (index < hrZones.length()); ++index) {
            const quint64 duration = getDuration(firstMap(hrZones
                .at(hrZones.length() - 3 + index).toMap().value(QLatin1String("duration"))));
            const QString hhmm = QString::fromLatin1("%1:%2")
                .arg(duration/1000/60, 2, 10, QLatin1Char('0'))
                .arg(duration/1000%60, 2, 10, QLatin1Char('0'));
            stream << "Timer" << (index+1) << "=" << hhmm << "\r\n";
        }

        /// @todo ActiveLimit - Need to parse *-phases file(s).

        const quint32 hrMax = first(firstMap(parsedPhysicalInformation.value(
            QLatin1String("maximum-heartrate"))).value(QLatin1String("value"))).toUInt();
        const quint32 hrRest = first(firstMap(parsedPhysicalInformation.value(
            QLatin1String("resting-heartrate"))).value(QLatin1String("value"))).toUInt();
        stream << "MaxHR="  << hrMax  << "\r\n";
        stream << "RestHR=" << hrRest << "\r\n";
        stream << "StartDelay=0\r\n"; ///< "Vantage NV RR data only".
        stream << "VO2max=" << first(firstMap(parsedPhysicalInformation.value(
            QLatin1String("vo2max"))).value(QLatin1String("value"))).toUInt() << "\r\n";
        stream << "Weight=" << first(firstMap(parsedPhysicalInformation.value(
            QLatin1String("weight"))).value(QLatin1String("value"))).toFloat() << "\r\n";

        // [Coach] "Coach parameters are only from Polar Coach HR monitor."

        // [Note]
        stream << "\r\n[Note]\r\n";
        if (parsedSession.contains(QLatin1String("note"))) {
            stream << first(firstMap(parsedSession.value(
                QLatin1String("note"))).value(QLatin1String("text"))).toString();
        } else if (parsedSession.contains(QLatin1String("session-name"))) {
            stream << first(firstMap(parsedSession.value(
                QLatin1String("session-name"))).value(QLatin1String("text"))).toString();
        } else {
            stream << "Exported by " << QApplication::applicationName()
                   << " " << QApplication::applicationVersion();
        }
        stream << "\r\n";

        // [HRZones]
        QMap<quint32, quint32> hrLimits; // Map hr-high to hr-low.
        foreach (const QVariant &entry, zones.value(QLatin1String("heartrate")).toList()) {
            const QVariantMap limits = firstMap(entry.toMap().value(QLatin1String("limits")));
            hrLimits.insert(
                first(limits.value(QLatin1String("high"))).toUInt(),
                first(limits.value(QLatin1String("low"))).toUInt());
        }
        // Limit to maximum of 10 HRZones (as implied by HRM v1.4).
        if (hrLimits.size() > 10) {
            hrZones.erase(hrZones.begin());
        }
        stream << "\r\n[HRZones]\r\n";
        const QList<quint32> hrLimitsKeys = hrLimits.keys();
        for (int index = hrLimitsKeys.length() - 2; index >=0; --index) {
            stream << hrLimitsKeys.at(index) << "\r\n"; // Zone 1 to n upper limits.
        }
        if (!hrLimits.empty()) {
            #if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
            stream << hrLimits.first() << "\r\n"; // Zone n lower limit.
            #else
            stream << hrLimits.constBegin().value() << "\r\n";
            #endif
        }
        for (int index = hrLimits.size() + (hrLimits.isEmpty() ? 1 : 0); index < 11; ++index) {
            stream << "0\r\n"; // "0" entries for a total of 11 HRZones entries.
        }

        /// @todo [SwapTimes] - Need to parse *-phases file(s).

        // [HRCCModeCh] "HR/CC mode swaps are a available only with Polar XTrainer Plus."

        // [IntTimes]
        bool isAutoLaps = false;
        QVariantList laps = manualLaps.value(QLatin1String("laps")).toList();
        if (laps.isEmpty()) {
            laps = autoLaps.value(QLatin1String("laps")).toList();
            isAutoLaps = true;
        }
        if (!laps.isEmpty()) {
            stream << "\r\n[IntTimes]\r\n";
            foreach (const QVariant &lap, laps) {
                const QVariantMap header = firstMap(lap.toMap().value(QLatin1String("header")));
                const QVariantMap stats = firstMap(lap.toMap().value(QLatin1String("stats")));
                const QVariantMap hrStats = firstMap(stats.value(QLatin1String("heartrate")));
                // Row 1
                stream << getDurationString(firstMap(header.value(QLatin1String("split-time"))));
                stream << '\t' << first(hrStats.value(QLatin1String("average"))).toUInt();
                stream << '\t' << first(hrStats.value(QLatin1String("minimum"))).toUInt();
                stream << '\t' << first(hrStats.value(QLatin1String("average"))).toUInt();
                stream << '\t' << first(hrStats.value(QLatin1String("maximum"))).toUInt();
                stream << "\r\n";
                // Row 2
                stream << "0\t0\t0\t0\t0\t0\r\n";
                // Row 3
                stream << "0\t0\t0";
                stream << '\t' << qRound(first(header.value(QLatin1String("ascent"))).toFloat() / 10.0);
                stream << '\t' << qRound(first(header.value(QLatin1String("distance"))).toFloat() / 100.0);
                stream << "\r\n";
                // Row 4
                switch (first(header.value(QLatin1String("lap-type"))).toInt()) {
                case 1:  stream << 1; break; // Distance -> interval
                case 2:  stream << 1; break; // Duration -> interval
                case 3:  stream << 0; break; // Location -> normal lap
                default: stream << 0; // Absent (ie manual) -> normal lap
                }
                stream << '\t' << qRound(first(header.value(QLatin1String("distance"))).toFloat());
                stream << '\t' << first(header.value(QLatin1String("power"))).toUInt();
                stream << '\t' << first(firstMap(stats.value(QLatin1String("temperature")))
                    .value(QLatin1String("average"))).toFloat();
                stream << "\t0"; // "Internal phase/lap information"
                stream << "\t0"; // Air pressure not available in protobuf data.
                stream << "\r\n";
                // Row 5
                stream << first(firstMap(stats.value(QLatin1String("stride")))
                    .value(QLatin1String("average"))).toUInt();
                stream << '\t' << (isAutoLaps ? '1' : '0');
                stream << "\t0\t0\t0\t0\r\n";
            }
        }

        // [IntNotes]
        if (!laps.isEmpty()) {
            stream << "\r\n[IntNotes]\r\n";
            for (int index = 0; index < laps.length(); ++index) {
                const QVariantMap header = firstMap(laps.at(index).toMap().value(QLatin1String("header")));
                switch (first(header.value(QLatin1String("lap-type"))).toInt()) {
                case 1:  stream << (index+1) << " Distance based lap\r\n"; break;
                case 2:  stream << (index+1) << " Duration based lap\r\n"; break;
                case 3:  stream << (index+1) << " Location based lap\r\n"; break;
                default: stream << (index+1) << " Manual lap\r\n";
                }
            }
        }

        /// @todo Need a canonical *-autolaps data file.
        /// @todo Add autolap type (eg distance / duration / location) note.

        // [ExtraData]
        /// @todo Can have up to 3 "extra" data series here, such as Lactate
        ///       and Power. Maybe Temperature? These are then included in
        ///       [IntTimes] above. Stride? Distance? Accelleration? Moving Type?

        /// @todo [Summary-123] - Need to parse *-phases file(s).
        stream << "\r\n[Summary-123]\r\n"; // WebSync includes 0's when empty.

        // [Summary-TH]
        const quint32 anaerobicThreshold = first(firstMap(parsedPhysicalInformation.value(
            QLatin1String("anaerobic-threshold"))).value(QLatin1String("value"))).toUInt();
        const quint32 aerobicThreshold = first(firstMap(parsedPhysicalInformation.value(
            QLatin1String("aerobic-threshold"))).value(QLatin1String("value"))).toUInt();
        const QVariantList heartrate = samples.value(QLatin1String("heartrate")).toList();
        int summaryThRow1[5] = { 0, 0, 0, 0, 0};
        for (int index = 0; index < heartrate.length(); ++index) {
            const quint32 hr = ((index < heartrate.length()) ? heartrate.at(index).toUInt() : (uint)0);
            if (hr > hrMax)
                summaryThRow1[0]++;
            else if (hr > anaerobicThreshold)
                summaryThRow1[1]++;
            else if (hr > aerobicThreshold)
                summaryThRow1[2]++;
            else if (hr > hrRest)
                summaryThRow1[3]++;
            else
                summaryThRow1[4]++;
        }
        stream << "\r\n[Summary-TH]\r\n"; // WebSync includes 0's when empty.
        stream << (heartrate.length() * qRound(recordInterval/1000.0f));
        for (size_t index = 0; index < (sizeof(summaryThRow1)/sizeof(summaryThRow1[0])); ++index) {
            stream << '\t' << (summaryThRow1[index] * qRound(recordInterval/1000.0f));
        }
        stream << "\r\n";
        stream << hrMax;
        stream << '\t' << anaerobicThreshold;
        stream << '\t' << aerobicThreshold;
        stream << '\t' << hrRest;
        stream << "\r\n";
        stream << "0\t" << heartrate.length() << "\r\n";

        // [Trip]
        stream << "\r\n[Trip]\r\n";
        stream << qRound(first(create.value(QLatin1String("distance"))).toFloat()/100.0) << "\r\n";
        stream << qRound(first(create.value(QLatin1String("ascent"))).toFloat()) << "\r\n";
        stream << qRound(getDuration(firstMap(create.value(QLatin1String("duration"))))/1000.0) << "\r\n";
        stream << qRound(first(firstMap(stats.value(QLatin1String("altitude"))).value(QLatin1String("average"))).toFloat()) << "\r\n";
        stream << qRound(first(firstMap(stats.value(QLatin1String("altitude"))).value(QLatin1String("maximum"))).toFloat()) << "\r\n";
        stream << qRound(first(firstMap(stats.value(QLatin1String("speed"))).value(QLatin1String("average"))).toFloat() * 128.0) << "\r\n";
        stream << qRound(first(firstMap(stats.value(QLatin1String("speed"))).value(QLatin1String("maximum"))).toFloat() * 128.0) << "\r\n";
        stream << "0\r\n"; // Odometer value at the end of an exercise.

        // [HRData]
        stream << "\r\n[HRData]\r\n";
        const QVariantList altitude = samples.value(QLatin1String("altitude")).toList();
        const QVariantList cadence  = samples.value(QLatin1String("cadence")).toList();
        const QVariantList speed    = samples.value(QLatin1String("speed")).toList();
        for (int index = 0; index < heartrate.length(); ++index) {
            stream << ((index < heartrate.length())
                ? heartrate.at(index).toUInt() : (uint)0);
            if (haveSpeed) {
                stream << '\t' << ((index < speed.length())
                    ? qRound(speed.at(index).toFloat() * 10.0) : ( int)0);
            }
            if (haveCadence) {
                stream << '\t' << ((index < cadence.length())
                    ? cadence.at(index).toUInt() : (uint)0);
            }
            if (haveAltitude) {
                stream << '\t' << ((index < altitude.length())
                    ? qRound(altitude.at(index).toFloat()) : ( int)0);
            }
            // Power (Watts) - not yet supported by Polar.
            // Power Balance and Pedalling Index - not yet supported by Polar.
            // Air pressure - not available in protobuf data.
            stream << "\r\n";
        }

        hrmList.append(hrmData);
    }
    return hrmList;
}

/**
 * @brief TrainingSession::toTCX
 *
 * @param buildTime If set, will override the internally detected build time.
 *                  Note, this is really only here to allow for deterministic
 *                  testing - not to be used by the final application.
 *
 * @return A TCX document representing the parsed Polar data.
 *
 * @see http://developer.garmin.com/schemas/tcx/v2/
 * @see http://www8.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd
 */
QDomDocument TrainingSession::toTCX(const QString &buildTime) const
{
    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction(QLatin1String("xml"),
        QLatin1String("version='1.0' encoding='utf-8'")));

    QDomElement tcx = doc.createElement(QLatin1String("TrainingCenterDatabase"));
    tcx.setAttribute(QLatin1String("xmlns"),
                     QLatin1String("http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2"));
    tcx.setAttribute(QLatin1String("xmlns:xsi"),
                     QLatin1String("http://www.w3.org/2001/XMLSchema-instance"));
    tcx.setAttribute(QLatin1String("xsi:schemaLocation"),
                     QLatin1String("http://www.garmin.com/xmlschemas/TrainingCenterDatabase/v2 "
                                   "http://www.garmin.com/xmlschemas/TrainingCenterDatabasev2.xsd"));
    doc.appendChild(tcx);

    QDomElement activities = doc.createElement(QLatin1String("Activities"));

    QDomElement multiSportSession;
    if ((parsedExercises.size() > 1) && (!parsedSession.isEmpty())) {
        multiSportSession = doc.createElement(QLatin1String("MultiSportSession"));
        multiSportSession.appendChild(doc.createElement(QLatin1String("Id")))
            .appendChild(doc.createTextNode(getDateTime(firstMap(parsedSession
                .value(QLatin1String("start")))).toString(Qt::ISODate)));
        activities.appendChild(multiSportSession);
    }

    foreach (const QVariant &exercise, parsedExercises) {
        const QVariantMap map = exercise.toMap();
        if (!map.contains(CREATE)) {
            qWarning() << "skipping exercise with no 'create' request data";
            continue;
        }
        const QVariantMap create = map.value(CREATE).toMap();
        const QVariantMap stats  = map.value(STATISTICS).toMap();

        QDomElement activity = doc.createElement(QLatin1String("Activity"));
        if (multiSportSession.isNull()) {
            activities.appendChild(activity);
        } else if (multiSportSession.childNodes().length() <= 1) {
            multiSportSession
                .appendChild(doc.createElement(QLatin1String("FirstSport")))
                .appendChild(activity);
        } else {
            multiSportSession
                .appendChild(doc.createElement(QLatin1String("NextSport")))
                .appendChild(activity);
        }
        Q_ASSERT(!activity.parentNode().isNull());

        // Get the sport type.
        activity.setAttribute(QLatin1String("Sport"),
            getTcxSport(first(firstMap(create.value(QLatin1String("sport")))
                .value(QLatin1String("value"))).toULongLong()));

        // Get the starting time.
        const QDateTime startTime = getDateTime(firstMap(create.value(QLatin1String("start"))));
        activity.appendChild(doc.createElement(QLatin1String("Id")))
            .appendChild(doc.createTextNode(startTime.toString(Qt::ISODate)));

        {
            QDomElement lap = doc.createElement(QLatin1String("Lap"));
            lap.setAttribute(QLatin1String("StartTime"), startTime.toString(Qt::ISODate));
            activity.appendChild(lap);

            lap.appendChild(doc.createElement(QLatin1String("TotalTimeSeconds")))
                .appendChild(doc.createTextNode(QString::fromLatin1("%1")
                    .arg(getDuration(firstMap(create.value(QLatin1String("duration"))))/1000.0)));
            lap.appendChild(doc.createElement(QLatin1String("DistanceMeters")))
                .appendChild(doc.createTextNode(QString::fromLatin1("%1")
                    .arg(first(create.value(QLatin1String("distance"))).toDouble())));
            lap.appendChild(doc.createElement(QLatin1String("MaximumSpeed")))
                .appendChild(doc.createTextNode(QString::fromLatin1("%1")
                    .arg(first(firstMap(stats.value(QLatin1String("speed")))
                        .value(QLatin1String("maximum"))).toDouble())));
            lap.appendChild(doc.createElement(QLatin1String("Calories")))
                .appendChild(doc.createTextNode(QString::fromLatin1("%1")
                    .arg(first(create.value(QLatin1String("calories"))).toUInt())));
            const QVariantMap hrStats = firstMap(stats.value(QLatin1String("heartrate")));
            lap.appendChild(doc.createElement(QLatin1String("AverageHeartRateBpm")))
                .appendChild(doc.createElement(QLatin1String("Value")))
                    .appendChild(doc.createTextNode(QString::fromLatin1("%1")
                        .arg(first(hrStats.value(QLatin1String("average"))).toUInt())));
            lap.appendChild(doc.createElement(QLatin1String("MaximumHeartRateBpm")))
                .appendChild(doc.createElement(QLatin1String("Value")))
                    .appendChild(doc.createTextNode(QString::fromLatin1("%1")
                        .arg(first(hrStats.value(QLatin1String("maximum"))).toUInt())));
            /// @todo Intensity must be one of: Active, Resting.
            lap.appendChild(doc.createElement(QLatin1String("Intensity")))
                .appendChild(doc.createTextNode(QString::fromLatin1("Active")));
            /// @todo [Optional] Cadence (ubyte, <=254).
            /// @todo TriggerMethod must be one of: Manual, Distance, Location, Time, HeartRate.
            lap.appendChild(doc.createElement(QLatin1String("TriggerMethod")))
                .appendChild(doc.createTextNode(QString::fromLatin1("Manual")));

            QDomElement track = doc.createElement(QLatin1String("Track"));
            lap.appendChild(track);

            // Add all the samples data to the track.
            addTrackSamples(doc, track, map, startTime);
        }
    }

    if ((multiSportSession.hasChildNodes()) ||
        (multiSportSession.isNull() && activities.hasChildNodes())) {
        tcx.appendChild(activities);
    }

    {
        QDomElement author = doc.createElement(QLatin1String("Author"));
        author.setAttribute(QLatin1String("xsi:type"), QLatin1String("Application_t"));
        author.appendChild(doc.createElement(QLatin1String("Name")))
            .appendChild(doc.createTextNode(QLatin1String("Bipolar")));
        tcx.appendChild(author);

        {
            QDomElement build = doc.createElement(QLatin1String("Build"));
            author.appendChild(build);
            QDomElement version = doc.createElement(QLatin1String("Version"));
            build.appendChild(version);
            QStringList versionParts = QApplication::applicationVersion().split(QLatin1Char('.'));
            while (versionParts.length() < 4) {
                versionParts.append(QLatin1String("0"));
            }
            version.appendChild(doc.createElement(QLatin1String("VersionMajor")))
                .appendChild(doc.createTextNode(versionParts.at(0)));
            version.appendChild(doc.createElement(QLatin1String("VersionMinor")))
                .appendChild(doc.createTextNode(versionParts.at(1)));
            version.appendChild(doc.createElement(QLatin1String("BuildMajor")))
                .appendChild(doc.createTextNode(versionParts.at(2)));
            version.appendChild(doc.createElement(QLatin1String("BuildMinor")))
                .appendChild(doc.createTextNode(versionParts.at(3)));
            QString buildType = QLatin1String("Release");
            VersionInfo versionInfo;
            const QString specialBuild = versionInfo.fileInfo(QLatin1String("SpecialBuild"));
            if (!specialBuild.isEmpty()) {
                buildType = specialBuild;
            }
            build.appendChild(doc.createElement(QLatin1String("Type")))
                .appendChild(doc.createTextNode(buildType));
            build.appendChild(doc.createElement(QLatin1String("Time")))
                .appendChild(doc.createTextNode(
                    buildTime.isEmpty() ? QString::fromLatin1(__DATE__" "__TIME__) : buildTime));
            /// @todo Fetch the login name at build time?
            build.appendChild(doc.createElement(QLatin1String("Builder")))
                .appendChild(doc.createTextNode(QLatin1String("Paul Colby")));
        }

        /// @todo  Make this dynamic if/when app is localized.
        author.appendChild(doc.createElement(QLatin1String("LangID")))
            .appendChild(doc.createTextNode(QLatin1String("EN")));
        author.appendChild(doc.createElement(QLatin1String("PartNumber")))
            .appendChild(doc.createTextNode(QLatin1String("434-F4C42-59")));
    }
    return doc;
}

void TrainingSession::addTrackSamples(QDomDocument &doc, QDomElement &track,
                                      const QVariantMap &map, const QDateTime &startTime,
                                      const int firstIndex, const int lastIndex) const
{
    // Get the "samples" samples.
    const QVariantMap samples = map.value(SAMPLES).toMap();
    const quint64 recordInterval = getDuration(
        firstMap(samples.value(QLatin1String("record-interval"))));
    const QVariantList altitude    = samples.value(QLatin1String("altitude")).toList();
    const QVariantList cadence     = samples.value(QLatin1String("cadence")).toList();
    const QVariantList distance    = samples.value(QLatin1String("distance")).toList();
    const QVariantList heartrate   = samples.value(QLatin1String("heartrate")).toList();
    const QVariantList speed       = samples.value(QLatin1String("speed")).toList();
    const QVariantList temperature = samples.value(QLatin1String("temperature")).toList();
    qDebug() << "samples sizes:"
        << altitude.size() << cadence.size() << distance.size()
        << heartrate.size() << speed.size() << temperature.size();

    // Get the "route" samples.
    const QVariantMap route = map.value(ROUTE).toMap();
    const QVariantList duration    = route.value(QLatin1String("duration")).toList();
    const QVariantList gpsAltitude = route.value(QLatin1String("altitude")).toList();
    const QVariantList latitude    = route.value(QLatin1String("latitude")).toList();
    const QVariantList longitude   = route.value(QLatin1String("longitude")).toList();
    const QVariantList satellites  = route.value(QLatin1String("satellites")).toList();
    qDebug() << "route sizes:" << duration.size() << gpsAltitude.size()
             << latitude.size() << longitude.size() << satellites.size();

    for (int index = firstIndex; (lastIndex < 0) || (index < lastIndex); ++index) {
        QDomElement trackPoint = doc.createElement(QLatin1String("Trackpoint"));

        if ((index < latitude.length()) && (index < longitude.length())) {
            QDomElement position = doc.createElement(QLatin1String("Position"));
            position.appendChild(doc.createElement(QLatin1String("LatitudeDegrees")))
                .appendChild(doc.createTextNode(latitude.at(index).toString()));
            position.appendChild(doc.createElement(QLatin1String("LongitudeDegrees")))
                .appendChild(doc.createTextNode(longitude.at(index).toString()));
            trackPoint.appendChild(position);
        }

        if ((index < altitude.length()) &&
            (!sensorOffline(samples.value(QLatin1String("altitude-offline")).toList(), index))) {
            trackPoint.appendChild(doc.createElement(QLatin1String("AltitudeMeters")))
                .appendChild(doc.createTextNode(altitude.at(index).toString()));
        }
        if ((index < distance.length()) &&
            (!sensorOffline(samples.value(QLatin1String("distance-offline")).toList(), index))) {
            trackPoint.appendChild(doc.createElement(QLatin1String("DistanceMeters")))
                .appendChild(doc.createTextNode(distance.at(index).toString()));
        }
        if ((index < heartrate.length()) && (heartrate.at(index).toInt() > 0) &&
            (!sensorOffline(samples.value(QLatin1String("heartrate-offline")).toList(), index))) {
            trackPoint.appendChild(doc.createElement(QLatin1String("HeartRateBpm")))
                .appendChild(doc.createElement(QLatin1String("Value")))
                .appendChild(doc.createTextNode(heartrate.at(index).toString()));
        }
        if ((index < cadence.length()) && (cadence.at(index).toInt() >= 0) &&
            (!sensorOffline(samples.value(QLatin1String("cadence-offline")).toList(), index))) {
            trackPoint.appendChild(doc.createElement(QLatin1String("Cadence")))
                .appendChild(doc.createTextNode(cadence.at(index).toString()));
        }

        if (trackPoint.hasChildNodes()) {
            #if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
            QDateTime trackPointTime = startTime.addMSecs(index * recordInterval);
            #else /// @todo Remove this hack when Qt 5.2+ is available on Travis CI.
            QDateTime trackPointTime = startTime.toUTC()
                .addMSecs(index * recordInterval).addSecs(startTime.utcOffset());
            trackPointTime.setUtcOffset(startTime.utcOffset());
            #endif
            trackPoint.insertBefore(doc.createElement(QLatin1String("Time")), QDomNode())
                .appendChild(doc.createTextNode(trackPointTime.toString(Qt::ISODate)));
            track.appendChild(trackPoint);
        } else {
            return; // We've exceeded the length of all data samples.
        }
    }
}

QByteArray TrainingSession::unzip(const QByteArray &data,
                                  const int initialBufferSize) const
{
    Q_ASSERT(initialBufferSize > 0);
    QByteArray result;
    result.resize(initialBufferSize);

    // Prepare a zlib stream structure.
    z_stream stream = {};
    stream.next_in = (Bytef *) data.data();
    stream.avail_in = data.length();
    stream.next_out = (Bytef *) result.data();
    stream.avail_out = result.size();
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    // Decompress the data.
    int z_result;
    for (z_result = inflateInit2(&stream, 15 + 32); z_result == Z_OK;) {
        if ((z_result = inflate(&stream, Z_SYNC_FLUSH)) == Z_OK) {
            const int oldSize = result.size();
            result.resize(result.size() * 2);
            stream.next_out = (Bytef *)(result.data() + oldSize);
            stream.avail_out = oldSize;
        }
    }

    // Check for errors.
    if (z_result != Z_STREAM_END) {
        qWarning() << "zlib error" << z_result << stream.msg;
        return QByteArray();
    }

    // Free any allocated resources.
    if ((z_result = inflateEnd(&stream)) != Z_OK) {
        qWarning() << "inflateEnd returned" << z_result << stream.msg;
    }

    // Return the decompressed data.
    result.chop(stream.avail_out);
    return result;
}

bool TrainingSession::writeGPX(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        qWarning() << "failed to open" << QDir::toNativeSeparators(fileName);
        return false;
    }
    return writeGPX(file);
}

bool TrainingSession::writeGPX(QIODevice &device)
{
    QDomDocument gpx = toGPX();
    if (gpx.isNull()) {
        qWarning() << "failed to conver to GPX" << baseName;
        return false;
    }
    device.write(gpx.toByteArray());
    return true;
}

QStringList TrainingSession::writeHRM(const QString &baseName)
{
    QStringList hrm = toHRM();
    if (hrm.isEmpty()) {
        qWarning() << "failed to conver to HRM" << baseName;
        return QStringList();
    }

    QStringList fileNames;
    for (int index = 0; index < hrm.length(); ++index) {
        const QString fileName = (hrm.length() == 1)
            ? QString::fromLatin1("%1.hrm").arg(baseName)
            : QString::fromLatin1("%1.%2.hrm").arg(baseName).arg(index);
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
            qWarning() << "failed to open" << QDir::toNativeSeparators(fileName);
        } else if (file.write(hrm.at(index).toLatin1())) {
            fileNames.append(fileName);
        }
    }
    return fileNames;
}

bool TrainingSession::writeTCX(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        qWarning() << "failed to open" << QDir::toNativeSeparators(fileName);
        return false;
    }
    return writeTCX(file);
}

bool TrainingSession::writeTCX(QIODevice &device)
{
    QDomDocument tcx = toTCX();
    if (tcx.isNull()) {
        qWarning() << "failed to conver to TCX" << baseName;
        return false;
    }
    device.write(tcx.toByteArray());
    return true;
}

}}

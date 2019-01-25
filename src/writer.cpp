#include "writer.h"
#include <QtMath>
//#include <QMap>
#include <QTextCodec>
#include <QTextDocument>
#include <QTextCursor>
#include <QPrinter>
#include <QRegularExpression>

namespace Writer
{
QString TimeToPT(const uint time, const double fps, const int timeStart)
{
    // Отделяем кадры от времени
    const int frames = timeStart % 1000;
    int newTime = static_cast<int>(time) + (timeStart - frames);

    // Пересчитываем кадры в миллисекунды
    const double tmpMsec = static_cast<double>(frames) * 1000.0 / fps;
    newTime += tmpMsec < 0 ? qFloor(tmpMsec) : qCeil(tmpMsec);

    // Запоминаем знак
    const bool negative = newTime < 0;
    newTime = abs(newTime);

    // Делим на компоненты
    const int hour = newTime / 3600000,
              min  = newTime / 60000 % 60,
              sec  = newTime / 1000  % 60,
              msec = newTime % 1000;

    // Собираем строку (последний компонент - кадры)
    const QChar fillChar = QChar('0');
    return QString("%1%2:%3:%4:%5")
            .arg(negative ? QString("−") : QString::null)
            .arg(hour, 2, 10, fillChar)
            .arg(min,  2, 10, fillChar)
            .arg(sec,  2, 10, fillChar)
            .arg(qFloor(static_cast<double>(msec) * fps / 1000.0), 2, 10, fillChar);
}

// Удаляет теги из текста фраз и объединяет соседние
PhraseList PreparePhrases(const Script::Script& script, const int joinInterval)
{
    const QString emptyActor = "[не размечено]";
    const QRegularExpression assTags("\\{[^\\}]*?\\}");

    PhraseList result;
    Phrase phrase;
    QString actor, text;
    bool first = true;
    for (const Script::Line::Event* const event : qAsConst(script.events.content))
    {
        actor = event->actorName.isEmpty() ? emptyActor : event->actorName; // Already trimmed
        text  = event->text.trimmed().replace("\\N", " ", Qt::CaseInsensitive).replace(assTags, QString::null);

        // Если интервал указан, фраза не первая, актёр совпадает и расстояние между фразами не более 5 сек.
        if (!first &&
            joinInterval > 0 &&
            actor == phrase.actor &&
            event->start >= phrase.end &&
            event->start - phrase.end <= static_cast<uint>(joinInterval))
        {
            phrase.end  = event->end;
            phrase.text += " ";
            phrase.text += text;
        }
        else
        {
            if (!first) result.append(phrase);

            phrase.start = event->start;
            phrase.end   = event->end;
            phrase.actor = actor;
            phrase.text  = text;

            first = false;
        }
    }
    if (!first) result.append(phrase);

    return result;
}

bool SaveSV(const Script::Script& script, const QString& fileName, const QStringList& actors, const double fps, const int timeStart, const int joinInterval, const QChar separator)
{
    const PhraseList phrases = PreparePhrases(script, joinInterval);

    // const int width = QString::number(rows.size()).size();
    // QMap<QString, uint> counters;
    // uint counter;
    // QString id;
    QString prevActor;
    QStringList line;
    QString result;
    for (const Phrase& phrase : phrases)
    {
        if ( actors.isEmpty() || actors.contains(phrase.actor, Qt::CaseInsensitive) )
        {
            // counter = counters.value(row->actor, 0) + 1;
            // counters[row->actor] = counter;
            // id = QString("%1%2").arg(row->actor).arg(counter, width, 10, QChar('0'));

            if (separator == SEP_CSV)
            {
                line.append( TimeToPT(phrase.start, fps, timeStart) );
                line.append( TimeToPT(phrase.end, fps, timeStart) );
                line.append( phrase.actor != prevActor ? phrase.actor : QString::null );
                line.append( phrase.text );
            }
            else if (separator == SEP_TSV)
            {
                line.append( phrase.actor );
                line.append( TimeToPT(phrase.start, fps, timeStart) );
            }

            for (QStringList::iterator str = line.begin(); str != line.end(); ++str)
            {
                if ( str->contains(separator) )
                {
                    (*str) = QString("\"%1\"").arg( str->replace(QChar('"'), "\"\"") );
                }
            }

            result.append( line.join(separator) );
            result.append( "\n" );
            line.clear();

            prevActor = phrase.actor;
        }
    }

    QFile fout(fileName);
    if (!fout.open(QFile::WriteOnly | QFile::Text)) return true;

    QTextStream out(&fout);
    out.setCodec( QTextCodec::codecForName("UTF-8") );
    out.setGenerateByteOrderMark(true);
    out << result;

    fout.close();
    return false;
}

void SavePDF(const Script::Script& script, const QString& fileName, const QStringList& actors, const double fps, const int timeStart, const int joinInterval)
{
    const PhraseList phrases = PreparePhrases(script, joinInterval);

    QTextDocument document;
    document.setDefaultFont(QFont("Helvetica", 14));
    QTextCursor cursor(&document);

    // const int width = QString::number(_rows.size()).size();
    // QMap<QString, uint> counters;
    // uint counter;
    bool first = true;
    for (const Phrase& phrase : phrases)
    {
        if ( actors.isEmpty() || actors.contains(phrase.actor, Qt::CaseInsensitive) )
        {
            // counter = counters.value(row->actor, 0) + 1;
            // counters[row->actor] = counter;
            // .arg(counter, width, 10, QChar('0'))

            if (!first)
            {
                cursor.insertBlock();
                cursor.insertBlock();
            }

            cursor.insertHtml( QString("<b>%1</b> %2<br/>%3")
                               .arg(phrase.actor)
                               .arg(TimeToPT(phrase.start, fps, timeStart))
                               .arg(phrase.text) );

            first = false;
        }
    }

    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setPaperSize(QPrinter::A4);
    printer.setOutputFileName(fileName);
    document.print(&printer);
}
}

/*
 * Code to interface with libcsv
 */

#include "csvprocessor.h"

#include <QDebug>
#include <QVector>
#include <QString>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <csv.h>

namespace GTFS {

// The bulk of this information comes from libcsv example(s)
// It's horribly OS-specific and nasty, it will have to do for now ... got myself in way over my head here :-(

// TODO: Implement in purely Qt / C++?

QVector<QString> localRecordHolder;

void cb1 (void *s, size_t i, void *outdata) {
    (void)i;
    (void)outdata;

    QString parsedItem = static_cast<char*>(s);
//    qDebug() << parsedItem;
    localRecordHolder.append(parsedItem);
}

void cb2 (int c, void *outdata) {
    (void)c;
//    qDebug() << "End of line: " << c;

    // Fill in the caller's vector with a new set of data
    QVector<QVector<QString>> *callerVec = static_cast<QVector<QVector<QString>> *>(outdata);
    callerVec->append(localRecordHolder);

    // Prepare for next record
    localRecordHolder.clear();
}


void CsvProcess(QString filename, QVector<QVector<QString>> *callerVec)
{
    char                buf[2048];
    size_t              i;
    struct csv_parser   p;
    FILE               *infile;

    // Purge the temporary area for record storage
    localRecordHolder.clear();

    csv_init(&p, CSV_APPEND_NULL);

    infile = fopen(filename.toUtf8(), "rb");

    if (infile == NULL) {
        qWarning() << "Bad file name: ERROR: " << filename;
        return;
    }

    while ((i = fread(buf, 1, 2048, infile)) > 0) {
        if (csv_parse(&p, buf, i, cb1, cb2, callerVec) != i) {
            qWarning() << "Error parsing file: " << filename << "ERROR CODE: " << csv_strerror(csv_error(&p));
            return;
        }
    }

    csv_fini(&p, cb1, cb2, callerVec);
    csv_free(&p);

    if (ferror(infile)) {
        qWarning() << "Error reading from input file: " << filename;
        return;
    }

    fclose(infile);

    // Just make double-sure we have cleared out the local record holder
    localRecordHolder.clear();
}

}


/*
 size_t csv_parse(struct csv_parser *p,
                  const void *s,
                  size_t len,
                  void (*cb1)(void *, size_t, void *),
                  void (*cb2)(int, void *),
                  void *data);
*/

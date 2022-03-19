/*
 * GtfsProc_Server
 * Copyright (C) 2018-2022, Daniel Brook
 *
 * This file is part of GtfsProc.
 *
 * GtfsProc is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * GtfsProc is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with GtfsProc.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 * See included LICENSE.txt file for full license.
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

/*
 * localRecordHolder stores each result from the libcsv callback
 */
static QVector<QString> localRecordHolder;

/*
 * cb1 is used to store each individual segment of a CSV file line (i.e. "the text between the commas")
 */
void cb1 (void *s, size_t i, void *outdata) {
    (void)i;
    (void)outdata;

    QString parsedItem = static_cast<char*>(s);
    localRecordHolder.append(parsedItem);
}

/*
 * cb2 is used to store the group of individual cb1 data into a single 'record'
 */
void cb2 (int c, void *outdata) {
    (void)c;

    // Fill in the caller's vector with a new set of data
    QVector<QVector<QString>> *callerVec = static_cast<QVector<QVector<QString>> *>(outdata);
    callerVec->append(localRecordHolder);

    // Prepare for next record
    localRecordHolder.clear();
}

/*
 * CsvProcess opens a CSV file and sends it to libcsv for processing
 */
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

    if (infile == nullptr) {
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

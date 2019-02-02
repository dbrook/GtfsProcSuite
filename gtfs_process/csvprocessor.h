/*
 * Code to interface with libcsv
 */

#ifndef CSVPROCESSOR_H
#define CSVPROCESSOR_H

#include <QString>

namespace GTFS {

void CsvProcess(QString filename, QVector<QVector<QString> > *callerVec);

}

#endif // CSVPROCESSOR_H

/********************************************************************************
* Copyright 2017 DigitalGlobe, Inc.
* Author: Kevin McGee
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
********************************************************************************/
#ifndef OPENSPACENET_QDEBUGSTREAM_H
#define OPENSPACENET_QDEBUGSTREAM_H

#include <iostream>
#include <streambuf>
#include <string>

#include <QObject>


class QDebugStream :  public QObject , public std::basic_streambuf<char>
{
    Q_OBJECT
public:
    QDebugStream(std::ostream &stream);
    QDebugStream();
    void setOptions(std::ostream &stream);
    ~QDebugStream();
    void eraseString();

protected:
    virtual int_type overflow(int_type v);
    virtual std::streamsize xsputn(const char *p, std::streamsize n);

private:
    std::ostream &m_stream;
    std::streambuf *m_old_buf;
    std::string m_string;

signals:
    void updateProgressText(QString);


};

#endif //OPENPACENET_QDEBUGSTREAM_H

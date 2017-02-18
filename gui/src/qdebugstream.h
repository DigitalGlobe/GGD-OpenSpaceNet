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
    QDebugStream(std::ostream &stream) : m_stream(stream)
    {
        //log_window = text_edit;
        m_old_buf = stream.rdbuf();
        stream.rdbuf(this);
    }

    QDebugStream() : m_stream(std::cerr)
    {

    }

    void setOptions(std::ostream &stream)
    {
        //log_window = text_edit;
        m_old_buf = stream.rdbuf();
        stream.rdbuf(this);
    }

    ~QDebugStream()
    {
        // output anything that is left
        if (!m_string.empty()) {
            //log_window->append(m_string.c_str());
            emit updateProgressText(QString::fromStdString(m_string.c_str()));
        }

        m_stream.rdbuf(m_old_buf);
    }

    void eraseString(){
        m_string.erase(m_string.begin(), m_string.end());
    }

protected:
    virtual int_type overflow(int_type v)
    {
        if (v == '\n' || v == '*')
        {
            //log_window->append(m_string.c_str());
            if (v == '*'){
                emit updateProgressText(QString::fromStdString("*"));
            }
            else
            {
                emit updateProgressText(QString::fromStdString(m_string.c_str()));
            }
            m_string.erase(m_string.begin(), m_string.end());
        }
        else
            m_string += v;

        return v;
    }

    virtual std::streamsize xsputn(const char *p, std::streamsize n)
    {
        m_string.append(p, p + n);

        int pos = 0;
        int posStar = 0;
        while (pos != std::string::npos)
        {
            pos = m_string.find('\n');
            posStar = m_string.find('*');
            if (pos != std::string::npos)
            {
                std::string tmp(m_string.begin(), m_string.begin() + pos);
                //log_window->append(tmp.c_str());
                emit updateProgressText(QString::fromStdString(m_string.c_str()));
                m_string.erase(m_string.begin(), m_string.begin() + pos + 1);
            }
            if(posStar != std::string::npos)
            {
                std::string tmp(m_string.begin(), m_string.begin() + posStar);
                //log_window->append(tmp.c_str());
                emit updateProgressText(QString::fromStdString(m_string.c_str()));
                m_string.erase(m_string.begin(), m_string.begin() + posStar + 1);
            }
        }

        return n;
    }

private:
    std::ostream &m_stream;
    std::streambuf *m_old_buf;
    std::string m_string;

signals:
    void updateProgressText(QString);


};

#endif //OPENPACENET_QDEBUGSTREAM_H

#ifndef OPENSKYNET_QDEBUGSTREAM_H
#define OPENSKYNET_QDEBUGSTREAM_H

#include <iostream>
#include <streambuf>
#include <string>

#include "qtextedit.h"
#include "processthread.h"

class QDebugStream : public std::basic_streambuf<char>
{
public:
    QDebugStream(std::ostream &stream, QTextEdit* text_edit) : m_stream(stream)
    {
        log_window = text_edit;
        m_old_buf = stream.rdbuf();
        stream.rdbuf(this);
    }

    QDebugStream() : m_stream(std::cout)
    {

    }

    void setOptions(std::ostream &stream, QTextEdit* text_edit, ProcessThread *thread)
    {
        log_window = text_edit;
        m_old_buf = stream.rdbuf();
        stream.rdbuf(this);
        threadp = thread;
    }

    ~QDebugStream()
    {
        // output anything that is left
        if (!m_string.empty()) {
            threadp.updateProgressInThread(m_string.c_str());
            //log_window->append(m_string.c_str());
        }

        m_stream.rdbuf(m_old_buf);
    }

protected:
    virtual int_type overflow(int_type v)
    {
        if (v == '\n')
        {
            threadp.updateProgressInThread(m_string.c_str());
            //log_window->append(m_string.c_str());
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
        while (pos != std::string::npos)
        {
            pos = m_string.find('\n');
            if (pos != std::string::npos)
            {
                std::string tmp(m_string.begin(), m_string.begin() + pos);
                threadp.updateProgressInThread(m_string.c_str());
                //log_window->append(tmp.c_str());
                m_string.erase(m_string.begin(), m_string.begin() + pos + 1);
            }
        }

        return n;
    }

private:
    std::ostream &m_stream;
    std::streambuf *m_old_buf;
    std::string m_string;
    ProcessThread threadp;

    QTextEdit* log_window;

};

#endif //OPENSKYNET_QDEBUGSTREAM_H

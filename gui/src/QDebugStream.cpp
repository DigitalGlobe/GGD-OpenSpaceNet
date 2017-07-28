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
#include "QDebugStream.h"


QDebugStream::QDebugStream(std::ostream &stream) : m_stream(stream)
{
	m_old_buf = stream.rdbuf();
	stream.rdbuf(this);
}

QDebugStream::QDebugStream() : m_stream(std::cerr)
{

}

void QDebugStream::setOptions(std::ostream &stream)
{
	m_old_buf = stream.rdbuf();
	stream.rdbuf(this);
}

QDebugStream::~QDebugStream()
{
	// output anything that is left
	if (!m_string.empty()) {
		emit updateProgressText(QString::fromStdString(m_string));
	}

	m_stream.rdbuf(m_old_buf);
}

void QDebugStream::eraseString(){
	m_string.erase(m_string.begin(), m_string.end());
}


std::basic_streambuf<char>::int_type QDebugStream::overflow(int_type v)
{
	if (v == '\n' || v == '*')
	{
		if (v == '*'){
			emit updateProgressText("*");
		}
		else
		{
			emit updateProgressText(QString::fromStdString(m_string));
		}
		m_string.erase(m_string.begin(), m_string.end());
	}
	else
		m_string += v;

	return v;
}

std::streamsize QDebugStream::xsputn(const char *p, std::streamsize n)
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
			emit updateProgressText(QString::fromStdString(m_string));
			m_string.erase(m_string.begin(), m_string.begin() + pos + 1);
		}
		if(posStar != std::string::npos)
		{
			std::string tmp(m_string.begin(), m_string.begin() + posStar);
			emit updateProgressText(QString::fromStdString(m_string));
			m_string.erase(m_string.begin(), m_string.begin() + posStar + 1);
		}
	}

	return n;
}


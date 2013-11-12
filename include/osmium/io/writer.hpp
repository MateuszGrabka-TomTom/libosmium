#ifndef OSMIUM_IO_WRITER_HPP
#define OSMIUM_IO_WRITER_HPP

/*

This file is part of Osmium (http://osmcode.org/osmium).

Copyright 2013 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <memory>
#include <string>

#include <osmium/io/output.hpp>
#include <osmium/io/compression.hpp>
#include <osmium/thread/debug.hpp>

namespace osmium {

    namespace io {

        class OutputThread {

            data_queue_type& m_input_queue;
            const std::string& m_compression;
            const int m_fd;

        public:

            OutputThread(data_queue_type& input_queue, const std::string& compression, int fd) :
                m_input_queue(input_queue),
                m_compression(compression),
                m_fd(fd) {
            }

            void operator()() {
                osmium::thread::set_thread_name("_osmium_output");

                std::unique_ptr<osmium::io::Compressor> compressor = osmium::io::CompressionFactory::instance().create_compressor(m_compression, m_fd);

                std::future<std::string> data_future;
                std::string data;
                do {
                    m_input_queue.wait_and_pop(data_future);
                    data = data_future.get();
                    compressor->write(data);
                } while (!data.empty());

                compressor->close();
            }

        }; // class OutputThread

        class Writer {

            osmium::io::File m_file;
            std::unique_ptr<osmium::io::Output> m_output;
            data_queue_type m_output_queue {};
            std::thread m_output_thread;

        public:

            Writer(const osmium::io::File& file, const osmium::io::Header& header = osmium::io::Header()) :
                m_file(file),
                m_output(osmium::io::OutputFactory::instance().create_output(m_file, m_output_queue)) {
                m_output->set_header(header);

                int fd = osmium::io::detail::open_for_writing(m_file.filename());

                m_output_thread = std::thread(OutputThread {m_output_queue, m_file.encoding()->compress(), fd});
            }

            Writer(const Writer&) = delete;
            Writer& operator=(const Writer&) = delete;

            ~Writer() {
                close();
                if (m_output_thread.joinable()) {
                    m_output_thread.join();
                }
            }

            void operator()(osmium::memory::Buffer&& buffer) {
                m_output->handle_buffer(std::move(buffer));
            }

            void close() {
                m_output->close();
            }

        }; // class Writer

    } // namespace io

} // namespace osmium

#endif // OSMIUM_IO_WRITER_HPP

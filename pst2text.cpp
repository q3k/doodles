// Copyright (c) 2015, Sergiusz 'q3k' Bazanski
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This is a small piece of shitty C++ code that tries to convert an
// Outlook PST file into a filesystem tree of plaintext messages.
// I am seriously not a C++ programmer, so this code is butt ugly and will
// crash horribly if you look at it funny. You have been warned.
//
// It converts the PST into a recursive tree like so:
// 
//  out/m-1234-0001/headers
//                 /body-text
//                 /body-html
//                 /subject
//                 /a/attach1.jpg
//                 /a/attach2.exe
//     /m-1234-0007/...
//     /f-subfolder/m-1234-0012/...
//                 /f-subfolder/...
//
// Python code to convert this to a real maildir is welcome :D.
// For now, use your unix-foo to browse and search this.
//
// To use:
//  - have boost & its' dev libraries installed
//  - get hold of pstsdk
//    (for verrsion 0.3.0 and a modern boost, replace line 132 of file
//    pstsdk/pst/message.h with an #if 0 - otherwise it won't compile)
//  - build this! For example,
//    g++ -std=c++11 pst2text.cpp -I ~/pstsdk_0_3_0 -o pst2text -O2
//  - run like so: (make sure the parent dir of outdir exists)
//    ./ps2text in.pst ~/outdir
//
// A static Linux amd64 build is available at:
//   https://q3k.org/pst2text

#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <utf8.h>
#include <string.h>
#include <stdio.h>
#include "pstsdk/pst.h"

inline void decode_utf8(const std::string& bytes, std::wstring& wstr)
{
    utf8::utf8to32(bytes.begin(), bytes.end(), std::back_inserter(wstr));
}

inline void encode_utf8(const std::wstring& wstr, std::string& bytes)
{
    utf8::utf32to8(wstr.begin(), wstr.end(), std::back_inserter(bytes));
}

void do_message(const pstsdk::message &message, int depth, std::wstring wpath)
{
    std::string headers, body, path;
    unsigned long long submit_date;
    encode_utf8(wpath, path);

    std::string subject;
    if (message.has_subject())
        encode_utf8(message.get_subject(), subject);
    else
        subject = "<unknown>";

    try
    {
        submit_date = message.get_property_bag().read_prop<unsigned long long>(0x39);
    }
    catch (pstsdk::key_not_found<unsigned short>)
    {
        std::cout << "No submit date in message '" << subject << "' - ignoring." << std::endl;
        return;
    }

    std::stringstream messagepath;
    // Do we need to do this? I am not good with computer.
    messagepath.clear();
    messagepath.str("");
    messagepath << path << "m-" << submit_date << "-" << message.get_id() << "/";

    mkdir(messagepath.str().c_str(), 0700);
    mkdir((messagepath.str() + "a/").c_str(), 0700);

    std::ofstream file_((messagepath.str() + "subject").c_str(), std::ios::out);
    file_ << subject;
    file_.close();
    if (message.has_body())
    {
        encode_utf8(message.get_body(), body);
        std::ofstream file((messagepath.str() + "body-text").c_str(), std::ios::out);
        file << body;
        file.close();
    }
    if (message.has_html_body())
    {
        // Apparently it's not really a wstring?! We have to fight the library here.
        // Also, I can't into C++. Please send help.
        std::vector<unsigned char> bytes = message.get_property_bag().read_prop<std::vector<unsigned char>>(0x1013);
        body = std::string((char *)&bytes[0]);
        std::ofstream file((messagepath.str() + "body-html").c_str(), std::ios::out);
        file << body;
        file.close();
    }
    try
    {
        encode_utf8(message.get_property_bag().read_prop<std::wstring>(0x7D), headers);
        std::ofstream file((messagepath.str() + "headers").c_str(), std::ios::out);
        file << headers;
        file.close();
    }
    catch (pstsdk::key_not_found<unsigned short>)
    {
    }

    for (auto ait = message.attachment_begin(); ait != message.attachment_end(); ++ait)
    {
        try
        {
            std::string attachment_name;
            encode_utf8((*ait).get_filename(), attachment_name);
            std::string sfname = (messagepath.str() + "a/" + attachment_name);
            const char *fname = sfname.c_str();

            FILE *f = fopen(fname, "w");
            std::vector<unsigned char> bytes = (*ait).get_bytes();
            fwrite(&bytes[0], bytes.size(), 1, f);
            fclose(f);
        }
        catch (...)
        {
            // gotta catch em all, weird errors happening here. Can't be assed to debug further.
        }
    }
}

void do_folder(const pstsdk::folder &folder, int depth, std::wstring wpath)
{
    std::string path, folder_name;
    encode_utf8(wpath, path);

    encode_utf8(folder.get_name(), folder_name);
    std::cout << std::string(depth*2, ' ') << "`-> " << folder_name << std::endl;

    mkdir(path.c_str(), 0700);

    for (auto itm = folder.message_begin(); itm != folder.message_end(); ++itm)
    {
        try
        {
            do_message(*itm, depth, wpath);
        }
        catch (...)
        {
            // PHP mode on!
            std::cout << "Unhandled error when parsing message." << std::endl;
        }
    }

    for (auto it = folder.sub_folder_begin(); it != folder.sub_folder_end(); ++it)
    {
        do_folder(*it, depth + 1, wpath + L"f-" + ((*it).get_name()) + L"/");
    }
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cout << "Usage: " << argv[0] << " in.pst /tmp/outdir/" << std::endl;
        return 1;
    }
    std::wstring pstfname, outdir;
    decode_utf8(std::string(argv[1]), pstfname);
    decode_utf8(std::string(argv[2]), outdir);
    if (outdir.back() != L'/')
        outdir += L"/";

    pstsdk::pst myfile(pstfname);
    auto fold = myfile.open_root_folder();    
    do_folder(fold, 0, outdir);
}


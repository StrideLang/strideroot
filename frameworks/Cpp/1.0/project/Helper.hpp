/*
    Stride is licensed under the terms of the 3-clause BSD license.

    Copyright (C) 2017. The Regents of the University of California.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

        Redistributions of source code must retain the above copyright notice,
        this list of conditions and the following disclaimer.

        Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

        Neither the name of the copyright holder nor the names of its
        contributors may be used to endorse or promote products derived from
        this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
    INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
    CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
    ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
    POSSIBILITY OF SUCH DAMAGE.

    Authors: Andres Cabrera and Joseph Tilbian
*/

#ifndef HELPER_HPP
#define HELPER_HPP

namespace stride {

template <class DataType>
class SignalHelper {
public:
    SignalHelper(DataType defaultValue = DataType()) {
        m_DefaultValue = defaultValue;
        init_Internal();
    }

    void process_OutputDomain(DataType *Output, DataType External) {
        m_Internal += External;
        *Output = m_Internal;
    }

    void process_PortDomain(DataType Input, DataType *Output) {
        *Output = Input;
    }

    void init_External(DataType *Output) {
        *Output = m_DefaultValue;
    }

    void init_Internal(void) {
        m_Internal = m_DefaultValue;
    }

private:
    DataType m_Internal;
    DataType m_DefaultValue;
};


template <class DataType>
class BundleHelper {
public:
    BundleHelper(int size) : m_Size(size) {
        m_Internal = new DataType[m_Size];
        init_Internal();
    }

    void process_OutputDomain(DataType *Output, DataType External[]) {
        DataType Sum = (DataType) 0.0;
        for (int i = 0; i < m_Size; i++) {
            m_Internal[i] += External[i];
            Sum += m_Internal[i];
        }
        *Output = Sum;
    }

    void process_PortDomain(DataType Input[], DataType *Output) {
        for (int i = 0; i < m_Size; i++) {
            Output[i] = Input[i];
        }
    }

    void init_External(DataType *Output) {
        for (int i = 0; i < m_Size; i++) {
            Output[i] = (DataType) 1.0;
        }
    }

    void init_Internal(void) {
        for (int i = 0; i < m_Size; i++) {
            m_Internal[i] = 0.0;
        }
    }

private:
    const int m_Size;
    DataType *m_Internal;
};

}

#endif // HELPER_HPP

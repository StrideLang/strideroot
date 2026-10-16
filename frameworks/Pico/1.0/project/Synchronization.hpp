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

#ifndef SYNCHRONIZATION_HPP
#define SYNCHRONIZATION_HPP

#include <mutex>

namespace stride {

namespace sync {

class scoped {};
class unscoped {};

class lock {};
class try_lock {};

template <class LockType>
class Synchronization {
public:
    // Unscoped Lock
    Synchronization(std::mutex *m, sync::lock, sync::unscoped) {
        m->lock();
    }

    // Unscoped TryLock
    Synchronization(std::mutex *m, sync::try_lock, sync::unscoped) {
        m_LockOwned = m->try_lock();
    }

    // Scoped Lock
    Synchronization(std::mutex *m, sync::lock, sync::scoped) : m_ScopedResetLock(*m) {
    }

    // Unscoped TryLock
    Synchronization(std::mutex *m, sync::try_lock, sync::scoped) : m_ScopedResetLock(*m, std::try_to_lock) {
        m_LockOwned = m_ScopedResetLock.owns_lock();
    }

    // Generic Lock Check
    bool operator()(LockType) {
        return true;
    }

private:
    std::unique_lock<std::mutex> m_ScopedResetLock;
    bool m_LockOwned;
};

// Specialized Lock Check for TryLock
template<> bool Synchronization<sync::try_lock>::operator()(sync::try_lock) {
    return m_LockOwned;
}

} // END NAMESPACE SYNC

} // END NAMESPACE STRIDE

#endif // SYNCHRONIZATION_HPP

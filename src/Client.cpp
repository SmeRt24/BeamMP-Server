// BeamMP, the BeamNG.drive multiplayer mod.
// Copyright (C) 2024 BeamMP Ltd., BeamMP team and contributors.
//
// BeamMP Ltd. can be contacted by electronic mail via contact@beammp.com.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Client.h"

#include "CustomAssert.h"
#include "TServer.h"
#include <memory>
#include <optional>

void TClient::DeleteCar(int Ident) {
    // TODO: Send delete packets
    std::unique_lock lock(mVehicleDataMutex);
    auto iter = std::find_if(mVehicleData.begin(), mVehicleData.end(), [&](auto& elem) {
        return Ident == elem.ID();
    });
    if (iter != mVehicleData.end()) {
        mVehicleData.erase(iter);
    } else {
        beammp_debug("tried to erase a vehicle that doesn't exist (not an error)");
    }
}

void TClient::ClearCars() {
    std::unique_lock lock(mVehicleDataMutex);
    mVehicleData.clear();
}

int TClient::GetOpenCarID() const {
    int OpenID = 0;
    bool found;
    std::unique_lock lock(mVehicleDataMutex);
    do {
        found = true;
        for (auto& v : mVehicleData) {
            if (v.ID() == OpenID) {
                OpenID++;
                found = false;
            }
        }
    } while (!found);
    return OpenID;
}

void TClient::AddNewCar(int Ident, const std::string& Data) {
    std::unique_lock lock(mVehicleDataMutex);
    mVehicleData.emplace_back(Ident, Data);
}

TClient::TVehicleDataLockPair TClient::GetAllCars() {
    return { &mVehicleData, std::unique_lock(mVehicleDataMutex) };
}

std::string TClient::GetCarPositionRaw(int Ident) {
    std::unique_lock lock(mVehiclePositionMutex);
    try {
        return mVehiclePosition.at(size_t(Ident));
    } catch (const std::out_of_range& oor) {
        beammp_debugf("Failed to get vehicle position for {}: {}", Ident, oor.what());
        return "";
    }
}

void TClient::Disconnect(std::string_view Reason) {
    beammp_debugf("Disconnecting client {} for reason: {}", GetID(), Reason);
    boost::system::error_code ec;
    if (mSocket.is_open()) {
        mSocket.shutdown(socket_base::shutdown_both, ec);
        if (ec) {
            beammp_debugf("Failed to shutdown client socket: {}", ec.message());
        }
        mSocket.close(ec);
        if (ec) {
            beammp_debugf("Failed to close client socket: {}", ec.message());
        }
    } else {
        beammp_debug("Socket is already closed.");
    }
}

void TClient::SetCarPosition(int Ident, const std::string& Data) {
    std::unique_lock lock(mVehiclePositionMutex);
    mVehiclePosition[size_t(Ident)] = Data;
}

std::string TClient::GetCarData(int Ident) {
    { // lock
        std::unique_lock lock(mVehicleDataMutex);
        for (auto& v : mVehicleData) {
            if (v.ID() == Ident) {
                return v.Data();
            }
        }
    } // unlock
    DeleteCar(Ident);
    return "";
}

void TClient::SetCarData(int Ident, const std::string& Data) {
    { // lock
        std::unique_lock lock(mVehicleDataMutex);
        for (auto& v : mVehicleData) {
            if (v.ID() == Ident) {
                v.SetData(Data);
                return;
            }
        }
    } // unlock
    DeleteCar(Ident);
}

int TClient::GetCarCount() const {
    // mVechileData holds both unicycle and cars which both count towards the maximum car count
    // spawning a unicycle meant reaching the max, hence being unable to spawn car. this dirty fixes the problem for now.
    std::unique_lock lock(mVehicleDataMutex);
    for (auto& v : mVehicleData) {
        if (v.ID() == mUnicycleID) {
            return int(mVehicleData.size() - 1);
        }
    }
    return int(mVehicleData.size());
}

TServer& TClient::Server() const {
    return mServer;
}

void TClient::EnqueuePacket(const std::vector<uint8_t>& Packet) {
    std::unique_lock Lock(mMissedPacketsMutex);
    mPacketsSync.push(Packet);
}

TClient::TClient(TServer& Server, ip::tcp::socket&& Socket)
    : mServer(Server)
    , mSocket(std::move(Socket))
    , mLastPingTime(std::chrono::high_resolution_clock::now()) {
}

TClient::~TClient() {
    beammp_debugf("client destroyed: {} ('{}')", this->GetID(), this->GetName());
}

void TClient::UpdatePingTime() {
    mLastPingTime = std::chrono::high_resolution_clock::now();
}
int TClient::SecondsSinceLastPing() {
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::high_resolution_clock::now() - mLastPingTime)
                       .count();
    return int(seconds);
}

std::optional<std::weak_ptr<TClient>> GetClient(TServer& Server, int ID) {
    std::optional<std::weak_ptr<TClient>> MaybeClient { std::nullopt };
    Server.ForEachClient([&](std::weak_ptr<TClient> CPtr) -> bool {
        ReadLock Lock(Server.GetClientMutex());
        if (!CPtr.expired()) {
            auto C = CPtr.lock();
            if (C->GetID() == ID) {
                MaybeClient = CPtr;
                return false;
            }
        } else {
            beammp_debugf("Found an expired client while looking for id {}", ID);
        }
        return true;
    });
    return MaybeClient;
}

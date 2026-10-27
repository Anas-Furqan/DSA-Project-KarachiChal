#include "core/Models.h"
#include <cmath>

using namespace std;

string rideTypeName(RideType t) {
    if (t == RideType::BIKE) return "BIKE";
    if (t == RideType::MINI) return "MINI";
    return "AC";
}

bool parseRideType(string text, RideType& out) {
    string upper = "";
    for (int i = 0; i < (int)text.size(); i++) {
        char c = text[i];
        if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';
        upper += c;
    }
    if (upper == "BIKE") {
        out = RideType::BIKE;
    } else if (upper == "MINI") {
        out = RideType::MINI;
    } else if (upper == "AC") {
        out = RideType::AC;
    } else {
        return false;
    }
    return true;
}

string driverStatusName(DriverStatus s) {
    if (s == DriverStatus::AVAILABLE) return "AVAILABLE";
    if (s == DriverStatus::TO_PICKUP) return "TO_PICKUP";
    return "ON_TRIP";
}

string rideStatusName(RideStatus s) {
    if (s == RideStatus::WAITING) return "WAITING";
    if (s == RideStatus::ASSIGNED) return "ASSIGNED";
    if (s == RideStatus::IN_PROGRESS) return "IN_PROGRESS";
    if (s == RideStatus::COMPLETED) return "COMPLETED";
    return "CANCELLED";
}

string eventTypeName(EventType e) {
    if (e == EventType::DRIVER_ARRIVED_PICKUP) return "DRIVER_ARRIVED_PICKUP";
    if (e == EventType::RIDE_COMPLETED) return "RIDE_COMPLETED";
    return "DRIVER_FREE";
}

int computeFare(RideType t, double km, double surge) {
    double base = 180;
    double perKm = 55;
    if (t == RideType::BIKE) {
        base = 60;
        perKm = 25;
    } else if (t == RideType::MINI) {
        base = 120;
        perKm = 40;
    }
    return (int)round((base + perKm * km) * surge);
}

string freeKey(int area, RideType t) {
    return to_string(area) + "-" + rideTypeName(t);
}

bool Motion::active() const {
    return legCount > 0;
}

double Motion::endTime() const {
    if (legCount == 0) return 0;
    return legEnd[legCount - 1];
}

int Motion::legAt(double t) const {
    for (int i = 0; i < legCount; i++) {
        if (t < legEnd[i]) return i;
    }
    return legCount - 1;
}

void Motion::clear() {
    legCount = 0;
}

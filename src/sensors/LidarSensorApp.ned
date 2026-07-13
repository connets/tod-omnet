//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
package tod_network.sensors;

simple LidarSensorApp like SensorBaseApp {
    parameters:
        @class(SensorBaseApp);
        int streamID = default(20);
        string sensorType = default("Lidar");
        volatile double dataSize @unit(B) = default(uniform(81000B, 99000B)); // ~90 KB ±10%
        double updateInterval @unit(s) = default(0.1s);                    // 10 Hz
        @display("i=block/source");
    gates:
        input fromManager;
        output toManager;
}

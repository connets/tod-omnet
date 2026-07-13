//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
package tod_network.sensors;

simple CameraSensorApp like SensorBaseApp {
    parameters:
        @class(SensorBaseApp);
        int streamID = default(4);
        string sensorType = default("Camera");
        volatile double dataSize @unit(B) = default(uniform(16000B, 24000B)); // ~20 KB ±20%
        double updateInterval @unit(s) = default(0s);                      // ogni frame
        @display("i=block/source");
    gates:
        input fromManager;
        output toManager;
}

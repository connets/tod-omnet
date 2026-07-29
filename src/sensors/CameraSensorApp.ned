//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
package tod_network.sensors;

simple CameraSensorApp like SensorBaseApp {
    parameters:
        @class(SensorBaseApp);
        int streamID = default(4);
        string sensorType = default("Camera");
        volatile double dataSize @unit(B) = default(uniform(16000B, 24000B)); // ~20 KB ±20% a 1280x720
        double updateInterval @unit(s) = default(0s);
        // Scala di qualita': dataSize vale al primo gradino, gli altri lo scalano
        // in proporzione ai pixel (960x540 = 56%, 640x360 = 25%, 320x180 = 6%).
        string qualityResolutions = default("1280x720 960x540 640x360 320x180");
        @display("i=block/source");
    gates:
        input fromManager;
        output toManager;
}

//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
package tod_network.sensors;

simple RadarSensorApp like SensorBaseApp {
    parameters:
        @class(SensorBaseApp);
        int streamID = default(24);
        string sensorType = default("Radar");
        volatile double dataSize @unit(B) = default(uniform(4500B, 5500B)); // ~5 KB ±10%
        double updateInterval @unit(s) = default(0.0666s);              // ~15 Hz
        // Non adattivo: la scala di qualita' e' pilotata dalla camera.
        string qualityResolutions = default("");
        @display("i=block/source");
    gates:
        input fromManager;
        output toManager;
}

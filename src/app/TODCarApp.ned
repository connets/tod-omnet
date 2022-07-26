package tod_network.app;

import inet.applications.udpapp.UdpBasicApp;


simple TODCarApp extends UdpBasicApp
{
    parameters:
        string todAgentType = default("normal");
        
        
}


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsdv-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numVehicles = 10;
    double highwayLength = 500.0; // meters
    double straightY = 50.0;      // y-coordinate for highway
    double simTime = 30.0;

    // Enable logging for UdpServer and UdpClient
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Nodes: vehicles + 1 RSU
    NodeContainer nodes;
    nodes.Create(numVehicles + 1);
    uint32_t rsuIdx = numVehicles;

    // Install Wifi 802.11p (WAVE) devices
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211p);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    NqosWaveMacHelper mac = NqosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();

    NetDeviceContainer devices = wifi80211p.Install(phy, mac, nodes);

    // Mobility: WaypointMobilityModel for vehicles, Fixed for RSU
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(nodes);

    Ptr<WaypointMobilityModel> rsuMob = nodes.Get(rsuIdx)->GetObject<WaypointMobilityModel>();
    rsuMob->AddWaypoint(Waypoint(Seconds(0.0), Vector(highwayLength / 2.0, straightY + 100.0, 0.0)));

    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<WaypointMobilityModel> vehMob = nodes.Get(i)->GetObject<WaypointMobilityModel>();
        double startX = 10.0 + i * (highwayLength - 20.0) / (numVehicles - 1);
        double endX = startX + 200.0; // Each vehicle drives 200 meters along x
        if (endX > highwayLength - 10.0) endX = highwayLength - 10.0;

        vehMob->AddWaypoint(Waypoint(Seconds(0.0), Vector(startX, straightY, 0.0)));
        vehMob->AddWaypoint(Waypoint(Seconds(simTime), Vector(endX, straightY, 0.0)));
    }

    // Internet stack + DSDV routing
    DsdvHelper dsdv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(dsdv);
    internet.Install(nodes);

    // IPv4 address assignment
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on RSU
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(rsuIdx));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Each vehicle is a UDP client sending packets to RSU
    for (uint32_t i = 0; i < numVehicles; ++i) {
        UdpClientHelper udpClient(interfaces.GetAddress(rsuIdx), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        udpClient.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(2.0 + (i*0.1)));
        clientApp.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
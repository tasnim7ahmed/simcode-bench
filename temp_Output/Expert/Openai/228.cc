#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    std::string sumoTraceFile = "sumo_mobility.tcl";
    uint32_t numVehicles = 10;
    double simTime = 80.0;
    cmd.AddValue("sumoTraceFile", "SUMO mobility trace file (TCL format)", sumoTraceFile);
    cmd.AddValue("numVehicles", "Number of vehicles", numVehicles);
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p = Wifi80211pHelper::Default();
    NetDeviceContainer wifiDevices = wifi80211p.Install(wifiPhy, wifiMac, vehicles);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::TraceMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(5),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicles);

    Ns2MobilityHelper ns2 = Ns2MobilityHelper(sumoTraceFile);
    ns2.Install();

    InternetStackHelper internet;
    DsrMainHelper dsrMain;
    DsrHelper dsr;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.0.0");
    Ipv4InterfaceContainer vehicleInterfaces = ipv4.Assign(wifiDevices);

    dsrMain.Install(dsr, vehicles);

    uint16_t port = 4000;

    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles.Get(numVehicles - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    UdpClientHelper client(vehicleInterfaces.GetAddress(numVehicles - 1), port);
    client.SetAttribute("MaxPackets", UintegerValue(320));
    client.SetAttribute("Interval", TimeValue(Seconds(0.25)));
    client.SetAttribute("PacketSize", UintegerValue(256));
    ApplicationContainer clientApps = client.Install(vehicles.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
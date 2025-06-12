#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t numVehicles = 10;
    double simTime = 80.0;
    std::string sumoTraces = "sumo-mobility.tcl";

    CommandLine cmd;
    cmd.AddValue("numVehicles", "Number of vehicles", numVehicles);
    cmd.AddValue("sumoTraces", "SUMO mobility trace file", sumoTraces);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create vehicle nodes
    NodeContainer vehicleNodes;
    vehicleNodes.Create(numVehicles);

    // Install wireless devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default();

    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, vehicleNodes);

    // Install internet stack with DSR routing
    InternetStackHelper internet;
    DsrMainHelper dsrMain;
    DsrHelper dsr;
    internet.Install(vehicleNodes);
    dsr.Install(internet, vehicleNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(wifiDevices);

    // Set up mobility using SUMO trace file
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::TraceMobilityModel");
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(10.0),
                                 "DeltaY", DoubleValue(10.0),
                                 "GridWidth", UintegerValue(5),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.Install(vehicleNodes);

    // Load SUMO mobility trace file for each vehicle
    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<Node> node = vehicleNodes.Get(i);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        Ptr<TraceMobilityModel> traceMob =
            DynamicCast<TraceMobilityModel>(mob);

        if (traceMob) {
            traceMob->LoadTrace(sumoTraces, i);
        }
    }

    // Set up UDP server on the last vehicle node
    uint16_t udpPort = 4000;
    UdpServerHelper udpServer(udpPort);
    ApplicationContainer serverApps = udpServer.Install(vehicleNodes.Get(numVehicles - 1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime));

    // Set up UDP client on the first vehicle node
    UdpClientHelper udpClient(interfaces.GetAddress(numVehicles - 1), udpPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = udpClient.Install(vehicleNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime));

    // Enable NetAnim output
    AnimationInterface anim("vanet-dsr.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
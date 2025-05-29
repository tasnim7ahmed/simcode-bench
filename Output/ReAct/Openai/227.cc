#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numUavs = 5;
    double simTime = 60.0;
    CommandLine cmd;
    cmd.AddValue("numUavs", "Number of UAV nodes", numUavs);
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.Parse(argc, argv);

    // Nodes: UAVs + 1 GCS
    NodeContainer uavNodes;
    uavNodes.Create(numUavs);
    NodeContainer gcsNode;
    gcsNode.Create(1);

    NodeContainer allNodes;
    allNodes.Add(gcsNode.Get(0));
    allNodes.Add(uavNodes);

    // WiFi 802.11n 2.4GHz
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager","DataMode","HtMcs7","ControlMode","HtMcs0");
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Mobility: UAVs in 3D, GCS static
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                  "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=500.0]"),
                                  "Z", StringValue ("ns3::UniformRandomVariable[Min=50.0|Max=120.0]"));
    mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue ("ns3::UniformRandomVariable[Min=8.0|Max=15.0]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.2]"),
                              "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(uavNodes);

    MobilityHelper gcsMobility;
    gcsMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    gcsMobility.Install(gcsNode);
    Ptr<MobilityModel> gcsModel = gcsNode.Get(0)->GetObject<MobilityModel>();
    gcsModel->SetPosition(Vector(250.0, 250.0, 0.0));

    // Internet stack with AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(allNodes);

    // IP addressing
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP Applications
    uint16_t port = 5000;
    // Install a UDP server on GCS
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(gcsNode.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Each UAV sends packets to GCS
    for (uint32_t i = 0; i < numUavs; ++i)
    {
        UdpClientHelper client(interfaces.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(320));
        client.SetAttribute("Interval", TimeValue(Seconds(0.18)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = client.Install(uavNodes.Get(i));
        clientApp.Start(Seconds(2.0 + i * 0.2));
        clientApp.Stop(Seconds(simTime));
    }

    wifiPhy.EnablePcapAll("uav-swarm");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
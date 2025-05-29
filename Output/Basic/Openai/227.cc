#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UavAodvSimulation");

int main(int argc, char *argv[])
{
    uint32_t nUavs = 5;
    double simTime = 60.0;
    CommandLine cmd;
    cmd.AddValue("nUavs", "Number of UAV (drone) nodes", nUavs);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    NodeContainer uavs;
    uavs.Create(nUavs);

    NodeContainer gcs;
    gcs.Create(1);

    NodeContainer allNodes;
    allNodes.Add(gcs);
    allNodes.Add(uavs);

    // Wifi PHY/MAC
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Mobility
    MobilityHelper mobility;

    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0)); // GCS at (0,0,0)
    for (uint32_t i = 0; i < nUavs; ++i)
    {
        double x = 100.0 * cos(2 * M_PI * i / nUavs);
        double y = 100.0 * sin(2 * M_PI * i / nUavs);
        double z = 50.0 + 10.0 * i;
        posAlloc->Add(Vector(x, y, z));
    }

    mobility.SetPositionAllocator(posAlloc);
    mobility.SetMobilityModel("ns3::GaussMarkovMobilityModel",
                              "Bounds", BoxValue(Box(-250, 250, -250, 250, 0, 200)),
                              "TimeStep", TimeValue(Seconds(1)),
                              "Alpha", DoubleValue(0.85),
                              "MeanVelocity", StringValue("ns3::ConstantRandomVariable[Constant=10.0]"),
                              "MeanDirection", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=6.2831853]"),
                              "MeanPitch", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"),
                              "NormalVelocity", StringValue("ns3::NormalRandomVariable[Mean=0.0|Variance=1.0]"),
                              "NormalDirection", StringValue("ns3::NormalRandomVariable[Mean=0.0|Variance=0.5]"),
                              "NormalPitch", StringValue("ns3::NormalRandomVariable[Mean=0.0|Variance=0.5]"));
    mobility.Install(allNodes);

    // Internet stack and AODV
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP applications: Each UAV sends to GCS
    uint16_t udpPort = 4000;

    for (uint32_t i = 1; i <= nUavs; ++i)
    {
        UdpClientHelper client(interfaces.GetAddress(0), udpPort);
        client.SetAttribute("MaxPackets", UintegerValue(0)); // unlimited
        client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        ApplicationContainer clientApp = client.Install(allNodes.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
    }

    UdpServerHelper server(udpPort);
    ApplicationContainer serverApp = server.Install(allNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Each UAV also receives from GCS
    for (uint32_t i = 1; i <= nUavs; ++i)
    {
        UdpServerHelper uavServer(udpPort + i);
        ApplicationContainer uavServerApp = uavServer.Install(allNodes.Get(i));
        uavServerApp.Start(Seconds(1.0));
        uavServerApp.Stop(Seconds(simTime));

        UdpClientHelper uavClient(interfaces.GetAddress(i), udpPort + i);
        uavClient.SetAttribute("MaxPackets", UintegerValue(0));
        uavClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        uavClient.SetAttribute("PacketSize", UintegerValue(256));
        ApplicationContainer uavClientApp = uavClient.Install(allNodes.Get(0));
        uavClientApp.Start(Seconds(2.0));
        uavClientApp.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime+1));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
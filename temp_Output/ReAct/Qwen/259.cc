#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("FiveGSimulation");

int main(int argc, char *argv[]) {
    uint32_t packetCount = 5;
    uint32_t packetSize = 1024;
    double simDuration = 10.0;

    // Create nodes
    NodeContainer gnbNode;
    gnbNode.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNode);

    mobility.SetMobilityModel("ns3::RandomWalk2DMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)),
                              "Distance", DoubleValue(10.0),
                              "Angle", StringValue("ns3::UniformRandomVariable[Min=0|Max=6.283185307]"));
    mobility.Install(ueNodes);

    // mmWave helper setup
    Ptr<MmWaveHelper> mmwaveHelper = CreateObject<MmWaveHelper>();
    mmwaveHelper->SetSchedulerType("ns3::MmWaveFlexTtiMacScheduler");
    mmwaveHelper->SetPhyAttribute("ChannelNumber", UintegerValue(0));
    mmwaveHelper->SetPhyAttribute("NChunkPerRB", UintegerValue(1));

    NetDeviceContainer gnbDevs = mmwaveHelper->InstallGnbDevice(gnbNode);
    NetDeviceContainer ueDevs = mmwaveHelper->InstallUeDevice(ueNodes);

    // Internet stack
    InternetStackHelper internet;
    internet.InstallAll();

    Ipv4AddressHelper ipv4; // Assign addresses in the 192.168.1.0/24 network
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ueInterfaces = ipv4.Assign(ueDevs);
    Ipv4InterfaceContainer gnbInterfaces = ipv4.Assign(gnbDevs);

    // Routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // UDP Echo Server on UE 2 (index 1)
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    // UDP Echo Client on UE 1 (index 0)
    UdpEchoClientHelper echoClient(ueInterfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(packetCount));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simDuration));

    // Simulation setup
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
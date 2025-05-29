#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MmWaveExample");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("MmWaveExample", LOG_LEVEL_INFO);

    // Create nodes: gNB and UEs
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-10, 10, -10, 10)));
    mobility.Install(ueNodes);

    // Install mmWave net devices
    MmWaveHelper mmwaveHelper;
    mmwaveHelper.SetEnbDeviceAttribute("DlEarfcn", UintegerValue(38400));
    NetDeviceContainer gnbDevs = mmwaveHelper.InstallGnb(gnbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper.InstallUe(ueNodes);

    // Install the internet stack
    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbIpIface = ipv4.Assign(gnbDevs);
    Ipv4InterfaceContainer ueIpIface = ipv4.Assign(ueDevs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create UDP echo server on UE2
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Create UDP echo client on UE1
    UdpEchoClientHelper echoClient(ueIpIface.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
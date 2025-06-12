#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/gnb-enb-device.h"
#include "ns3/point-to-point-module.h"
#include "ns3/udp-client-server-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Simple5GNetwork");

int main(int argc, char* argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponent::SetDefaultLogLevel(LOG_LEVEL_INFO);
    LogComponent::SetPrintLinePrefix(true);

    // Create nodes: UE, gNB, and CN
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer cnNodes;
    cnNodes.Create(1);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gnbNodes);
    internet.Install(cnNodes);

    // Point-to-point link between gNB and CN
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer gnbCnDevices;
    gnbCnDevices = p2p.Install(gnbNodes.Get(0), cnNodes.Get(0));

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbCnInterfaces;
    gnbCnInterfaces = address.Assign(gnbCnDevices);

    Ipv4Address ueIpAddress("10.1.2.2");
    Ipv4Address gnbIpAddress("10.1.2.1");

    // Set up addresses on UE and gNB (using a dummy p2p link)
    PointToPointHelper p2pUeGnb;
    p2pUeGnb.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2pUeGnb.SetChannelAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer ueGnbDevices;
    ueGnbDevices = p2pUeGnb.Install(ueNodes.Get(0), gnbNodes.Get(0));
    Ipv4InterfaceContainer ueGnbInterfaces;

    address.SetBase("10.1.2.0", "255.255.255.0");
    ueGnbInterfaces = address.Assign(ueGnbDevices);


    // UDP server (UE)
    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client (gNB)
    UdpClientHelper client(ueGnbInterfaces.GetAddress(0), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(gnbNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
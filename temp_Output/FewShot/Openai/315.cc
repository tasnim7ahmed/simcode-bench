#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/nr-helper.h"
#include "ns3/epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Set logging for client and server
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: UE and gNB
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    // Create NR helper and EPC helper
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Create NR device helper
    nrHelper->SetAttribute("Scheduler", StringValue("ns3::NrMacSchedulerTdmaRR"));

    // Deployment configuration
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbNodes);
    mobility.Install(ueNodes);

    // Install NR devices to gNB and UE
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes);

    // Attach UE to gNB
    nrHelper->AttachToClosestEnb(ueNetDev, gnbNetDev);

    // Assign IP to UEs via EPC
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(gnbNodes);
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));

    // Add default route for UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(ueNode->GetObject<Ipv4>()->GetRoutingProtocol());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Remote host (to represent CN)
    Ptr<Node> remoteHost = CreateObject<Node>();
    internet.Install(remoteHost);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Add(remoteHost);

    // Connect remoteHost to EPC PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);

    // Add static route to remoteHost
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting<Ipv4StaticRouting>(remoteHost->GetObject<Ipv4>()->GetRoutingProtocol());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install applications: UDP server on UE
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client on remoteHost (through gNB, CN to UE)
    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(remoteHost);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create nodes: eNB (acts as UDP client), UE (acts as UDP server)
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install Mobility Model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Create LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the internet stack on UE
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP Address to UEs
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = lteHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach UE to eNB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Add routing for UE
    Ptr<Node> ueNode = ueNodes.Get(0);
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
    ueStaticRouting->AddNetworkRouteTo(Ipv4Address("1.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Set up UDP Server on UE
    uint16_t port = 12345;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(25.0));

    // Create a remote host to send UDP packets (simulates eNB's client behind the EPC)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // Set up internet stack for remote host
    internet.Install(remoteHostContainer);

    // Create point-to-point link from remote host to the EPC
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(remoteHost, lteHelper->GetEpcHelper()->GetPgwNode());

    // Assign IP address for remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer remoteHostIpIfaces = ipv4h.Assign(internetDevices);

    // Add static routing for remote host towards UE subnet via EPC
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Set up UDP Client on remote host (sending to UE)
    UdpClientHelper udpClient(ueIpIface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = udpClient.Install(remoteHost);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(25.0));

    // Enable LTE summary output
    lteHelper->EnableTraces();

    // Run simulation
    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/lte-module.h"
#include "ns3/epc-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: 1 eNodeB, 2 UEs
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNode;
    enbNode.Create(1);

    // Create EPC and LTE helpers
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
    lteHelper->SetEpcHelper(epcHelper);

    // Install Mobility Model in grid layout
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (0.0),
                                  "MinY", DoubleValue (0.0),
                                  "DeltaX", DoubleValue (20.0),
                                  "DeltaY", DoubleValue (20.0),
                                  "GridWidth", UintegerValue (2),
                                  "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);
    mobility.Install(ueNodes);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP addresses to UEs, and add routing
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Install remote host to serve as anchor for downlink traffic (not strictly needed for UE-to-UE UDP here)
    // But EPC requires remote host setup
    Ptr<Node> remoteHost;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHostContainer);

    // Link remote host to PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
    p2ph.SetChannelAttribute ("Delay", TimeValue (Seconds (0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    // Assign a default route to remotehost in order to reach UEs IP
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo ("7.0.0.0", "255.0.0.0", 1);

    // Set up UDP server on UE 2
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Set up UDP client on UE 1, to send 2 packets of 512 bytes each to UE 2
    UdpClientHelper udpClient(ueIpIfaces.GetAddress(1), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(2));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/csma-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: eNodeB and 2 UEs
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer enbNodes;
    enbNodes.Create(1);

    // Install mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // eNodeB
    positionAlloc->Add(Vector(50.0, 0.0, 0.0));   // UE 0
    positionAlloc->Add(Vector(100.0, 0.0, 0.0));  // UE 1
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE devices
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IPv4 addresses to UEs
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // EPC creates a single node: PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create a remote host behind a CSMA network
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    InternetStackHelper internetHost;
    internetHost.Install(remoteHostContainer);
    CsmaHelper csma;
    NodeContainer csmaNodes;
    csmaNodes.Add(pgw);
    csmaNodes.Add(remoteHostContainer.Get(0));
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    csma.SetChannelAttribute("Delay", TimeValue(Seconds(0.000001)));
    NetDeviceContainer csmaDevs = csma.Install(csmaNodes);

    // Assign IP address to remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(csmaDevs);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    // Attach UEs to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Set default gateway for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> uerouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ueNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        uerouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Applications

    // TCP Server (PacketSink) on UE 0, port 8080
    uint16_t serverPort = 8080;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), serverPort));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

    ApplicationContainer sinkApps = sinkHelper.Install(ueNodes.Get(0));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(10.0));

    // TCP Client (OnOff) on UE 1, target is UE 0 IP at port 8080, sends 1,000,000 bytes
    uint32_t maxBytes = 1000000;
    Ipv4Address serverAddress = ueIpIfaces.GetAddress(0);

    OnOffHelper client ("ns3::TcpSocketFactory", InetSocketAddress(serverAddress, serverPort));
    client.SetAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    client.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer clientApps = client.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/epc-helper.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes: UE, gNB
    NodeContainer ueNodes;
    ueNodes.Create(1);
    NodeContainer gnbNodes;
    gnbNodes.Create(1);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Create the NR helper and EPC helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    // Create core network (CN) node
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create NR sectorization
    nrHelper->SetNrDeviceAttribute("DlBandwidth", UintegerValue(100));
    nrHelper->SetNrDeviceAttribute("UlBandwidth", UintegerValue(100));

    // Use a single cell with 1 sector
    NodeContainer allNodes;
    allNodes.Add(ueNodes);
    allNodes.Add(gnbNodes);

    // Position Allocators
    Ptr<ListPositionAllocator> gnbPositionAlloc = CreateObject<ListPositionAllocator>();
    gnbPositionAlloc->Add(Vector(0.0, 0.0, 10.0));

    Ptr<ListPositionAllocator> uePositionAlloc = CreateObject<ListPositionAllocator>();
    uePositionAlloc->Add(Vector(100.0, 0.0, 1.5));

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.SetPositionAllocator(gnbPositionAlloc);
    mobility.Install(gnbNodes);

    mobility.SetPositionAllocator(uePositionAlloc);
    mobility.Install(ueNodes);

    // Install NR devices
    NetDeviceContainer gnbDevs = nrHelper->InstallGnbDevice(gnbNodes);
    NetDeviceContainer ueDevs = nrHelper->InstallUeDevice(ueNodes);

    // Attach UEs to gNB
    nrHelper->AttachToClosestEnb(ueDevs, gnbDevs);

    // Assign IP address to UEs
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set up default route for UEs
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(ueNodes.Get(i)->GetObject<Ipv4>()->GetRoutingProtocol());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Create a remote host as the traffic generator/payload sink for interconnection via PGW
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internetHostHelper;
    internetHostHelper.Install(remoteHost);

    // Set up the remote P2P link between remoteHost and PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP address to remoteHost and PGW
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Add routing on remoteHost
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting>(remoteHost->GetObject<Ipv4>()->GetRoutingProtocol());
    remoteHostStaticRouting->AddNetworkRouteTo("7.0.0.0", "255.0.0.0", 1);

    // UDP server on UE (port 9)
    uint16_t port = 9;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // UDP client on gNB, sending to UE's IP address
    Ipv4Address ueAddr = ueIpIfaces.GetAddress(0);

    UdpClientHelper udpClient(ueAddr, port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(1000));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.01)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    // We run the client on the remoteHost, sending to UE via core/gNB
    ApplicationContainer clientApps = udpClient.Install(remoteHost);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2ph.EnablePcapAll("nr-simple");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes: one eNB and one UE
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Install Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // eNB
    positionAlloc->Add(Vector(50.0, 0.0, 0.0)); // UE
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Create LTE devices and helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Install eNB devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);

    // Install UE devices
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Assign IP address to UEs, get the UE IP
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to eNB
    lteHelper->Attach(ueLteDevs.Get(0), enbLteDevs.Get(0));

    // Set up default gateway
    uint16_t dlPort = 12345;

    // Application: UDP Server on UE
    UdpServerHelper server(dlPort);
    ApplicationContainer serverApps = server.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(25.0));

    // Application: UDP Client on remote host (acting as eNB-side client)
    // Create remote host attached to SGW/PGW
    Ptr<Node> remoteHost;
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    remoteHost = remoteHostContainer.Get(0);
    internet.Install(remoteHostContainer);

    // Connect remote host to PGW
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gbps")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    // interface 1 is for remote host
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set up routing
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // UDP Client: send to the UE's IP address (lte-assigned IP)
    UdpClientHelper client(ueIpIface.GetAddress(0), dlPort);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = client.Install(remoteHost);
    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(25.0));

    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(25.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
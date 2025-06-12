#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    double simDuration = 10.0; // seconds

    // Create nodes
    NodeContainer ueNodes;
    ueNodes.Create(2);

    NodeContainer enbNode;
    enbNode.Create(1);

    NodeContainer remoteHosts;
    remoteHosts.Create(1);

    // LTE helper setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Spectrum model (default)
    lteHelper->SetPathlossModelType(TypeId::LookupByName("ns3::FriisSpectrumPropagationLossModel"));

    // Install EPC and LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack on UEs and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(remoteHosts);

    // Assign IP addresses to UEs
    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to the eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Get PGW node and connect remote host via point-to-point link
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHosts.Get(0));
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Set up routing so remote host can reach UE subnet
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostRouting = ipv4RoutingHelper.GetStaticRouting(remoteHosts.Get(0)->GetObject<Ipv4>());
    remoteHostRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Install UDP echo server on remote host
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(remoteHosts.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    // Install UDP echo clients on UEs
    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);
    UdpEchoClientHelper echoClient(internetIpIfaces.GetAddress(1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100000));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(ueNodes);
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simDuration));

    // Mobility model: static positions
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(enbNode);
    mobility.Install(remoteHosts);

    // Animation output
    AnimationInterface anim("lte_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));
    anim.EnablePacketMetadata(true);

    // Start simulation
    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
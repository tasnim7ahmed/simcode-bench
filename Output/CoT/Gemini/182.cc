#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    Config::SetDefault ("ns3::LteEnbComponentCarrierUeManager::DlSchedulerType", StringValue ("ns3::RrFfMacScheduler"));
    Config::SetDefault ("ns3::LteEnbComponentCarrierUeManager::UlSchedulerType", StringValue ("ns3::RrFfMacScheduler"));

    uint16_t numberOfUes = 2;

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);

    // Create EPC helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a single remote host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create the Internet
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("100Gb/s"));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer internetDevices = p2ph.Install(remoteHost, epcHelper->GetPgwNode());
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(epcHelper->GetMmeSgwPgwAddress(), Ipv4Mask("255.255.255.0"), 1);

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to the closest eNodeB
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    // Set active UEs
    lteHelper->SetActiveUe(ueLteDevs);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetMmeSgwPgwAddress(), 1);
    }

    // Create Applications
    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;

    // Install a UdpEchoServer on the remote host
    UdpEchoServerHelper echoServerHelper(dlPort);
    ApplicationContainer serverApps = echoServerHelper.Install(remoteHost);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install a UdpEchoClient on the first UE
    UdpEchoClientHelper echoClientHelper(remoteHostAddr, dlPort);
    echoClientHelper.SetAttribute("MaxPackets", UintegerValue(100));
    echoClientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    echoClientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClientHelper.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Install BulkSendApp on the second UE
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", InetSocketAddress(remoteHostAddr, ulPort));
    bulkSendHelper.SetAttribute("SendSize", UintegerValue(1024));
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(1000000));
    ApplicationContainer bulkSendApps = bulkSendHelper.Install(ueNodes.Get(1));
    bulkSendApps.Start(Seconds(3.0));
    bulkSendApps.Stop(Seconds(10.0));

    // Sink Application on the remote host
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), ulPort));
    ApplicationContainer sinkApps = sinkHelper.Install(remoteHost);
    sinkApps.Start(Seconds(2.5));
    sinkApps.Stop(Seconds(10.0));

    // Enable PCAP tracing
    // p2ph.EnablePcapAll("lte-epc-first");

    // Enable NetAnim
    AnimationInterface anim("lte-epc-first.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(ueNodes.Get(0), 20.0, 20.0);
    anim.SetConstantPosition(ueNodes.Get(1), 30.0, 20.0);
    anim.SetConstantPosition(remoteHost, 40.0, 10.0);

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
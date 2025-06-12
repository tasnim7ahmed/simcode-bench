#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("LteEnbNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("LteUeNetDevice", LOG_LEVEL_INFO);

    // Create Nodes: eNodeB, UEs, and a remote host
    NodeContainer enbNodes;
    enbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);

    // Create EPC Helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Install LTE devices to the nodes
    NetDeviceContainer enbNetDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueNetDevs = lteHelper->InstallUeDevice(ueNodes);

    // Configure Mobility Models
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);

    // Install the IP stack on the nodes
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);
    internet.Install(ueNodes);

    // Install the internet stack on the EPC
    epcHelper->InstallInternetStack(remoteHostContainer);

    // Connect eNodeB to the EPC
    NetDeviceContainer enbNetDevsEpc;
    enbNetDevsEpc = epcHelper->InstallEnbDevice(enbNodes);

    // PointToPoint link between the remote host and the PGW
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetDeviceAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer p2pDevices = p2p.Install(remoteHostContainer.Get(0), epcHelper->GetPgwNode());

    // Assign IP address to the remote host
    Ipv4AddressHelper ip;
    ip.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIfaces = ip.Assign(p2pDevices);
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(0);

    // Set default gateway for UE
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        Ptr<Node> ue = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetPgwAddress(), 1);
    }

    // Attach UEs to the eNodeB
    lteHelper->Attach(ueNetDevs.Get(0), enbNetDevs.Get(0));
    lteHelper->Attach(ueNetDevs.Get(1), enbNetDevs.Get(0));

    // Activate a data radio bearer between UE and eNodeB
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    lteHelper->ActivateDataRadioBearer(ueNodes.Get(0), bearer);
    lteHelper->ActivateDataRadioBearer(ueNodes.Get(1), bearer);

    // Create UDP application
    uint16_t dlPort = 10000;
    UdpClientHelper clientHelper(remoteHostAddr, dlPort);
    clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    clientHelper.SetAttribute("MaxPackets", UintegerValue(1000));

    ApplicationContainer clientApps;
    clientApps.Add(clientHelper.Install(ueNodes.Get(0)));
    clientApps.Add(clientHelper.Install(ueNodes.Get(1)));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    // Create a server application on the remote host
    UdpServerHelper serverHelper(dlPort);
    ApplicationContainer serverApps = serverHelper.Install(remoteHostContainer.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Enable PCAP tracing
    p2p.EnablePcapAll("lte-simulation");

    // Set simulation stop time
    Simulator::Stop(Seconds(10.0));

    // NetAnim
    AnimationInterface anim("lte-simulation.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 10.0, 10.0);
    anim.SetConstantPosition(ueNodes.Get(0), 15.0, 15.0);
    anim.SetConstantPosition(ueNodes.Get(1), 5.0, 5.0);
    anim.SetConstantPosition(remoteHostContainer.Get(0), 20.0, 10.0);

    // Run the simulation
    Simulator::Run();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    for (auto i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
        std::cout << "  Delay Sum: " << i->second.delaySum << "\n";

    }
    monitor->SerializeToXmlFile("lte-simulation.flowmon", true, true);


    Simulator::Destroy();
    return 0;
}
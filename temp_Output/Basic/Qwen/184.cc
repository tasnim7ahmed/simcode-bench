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

    // Create eNodeB and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Create remote host node
    NodeContainer remoteHost;
    remoteHost.Create(1);

    // Setup LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set path loss model
    lteHelper->SetPathlossModelType(TypeId::FindByName("ns3::FriisSpectrumPropagationLossModel"));

    // Install LTE devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install internet stack on UEs and remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(remoteHost);

    // Attach UEs to the eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(ueDevs);
    Ipv4Address remoteHostAddr = epcHelper->GetPgwNode()->GetObject<Ipv4>()->GetInterface(0)->GetAddress(0).GetLocal();

    // Connect remote host to PGW via point-to-point link
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.001)));
    NetDeviceContainer internetDevices = p2ph.Install(epcHelper->GetPgwNode(), remoteHost.Get(0));
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    // Set default gateway for remote host
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost.Get(0)->GetObject<Ipv4>());
    remoteHostRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Setup UDP echo server on remote host
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(remoteHost.Get(0));
    serverApps.Start(Seconds(0.1));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP echo clients on UEs
    uint32_t packetSize = 1024;
    Time interPacketInterval = MilliSeconds(100);
    UdpEchoClientHelper echoClient(remoteHostAddr, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps1 = echoClient.Install(ueNodes.Get(0));
    clientApps1.Start(Seconds(1.0));
    clientApps1.Stop(Seconds(10.0));

    ApplicationContainer clientApps2 = echoClient.Install(ueNodes.Get(1));
    clientApps2.Start(Seconds(2.0));
    clientApps2.Stop(Seconds(10.0));

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNodes);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(ueNodes);
    for (auto it = ueNodes.Begin(); it != ueNodes.End(); ++it) {
        Ptr<MobilityModel> mm = (*it)->GetObject<MobilityModel>();
        Vector position = mm->GetPosition();
        mm->SetPosition(position);
        mm->SetVelocity(Vector(1.0, 0.0, 0.0));
    }

    mobility.Install(remoteHost);

    // Animation interface
    AnimationInterface anim("lte-simple-animation.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 0.0, 0.0);
    anim.SetConstantPosition(ueNodes.Get(0), 10.0, 0.0);
    anim.SetConstantPosition(ueNodes.Get(1), 20.0, 0.0);
    anim.SetConstantPosition(remoteHost.Get(0), 30.0, 0.0);

    // Enable PCAP tracing
    p2ph.EnablePcapAll("lte-simple");

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Simulation parameters
    double simDurationSeconds = 10.0;
    double distanceBetweenNodesMeters = 60.0;

    // Create nodes: 2 UEs and 1 eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(2);

    NodeContainer enbNode;
    enbNode.Create(1);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(enbNode);

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(ueNodes);

    // LTE helper setup
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Spectrum and channel settings
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Install LTE devices
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNode);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach UEs to the first eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Set TCP as the transport protocol
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Install applications
    uint16_t dlPort = 1234;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    // Server application on UE 0
    UdpEchoServerHelper echoServer(dlPort);
    serverApps = echoServer.Install(ueNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simDurationSeconds));

    // Client application on UE 1
    UdpEchoClientHelper echoClient(ueIpIfaces.GetAddress(0), dlPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(100));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    clientApps = echoClient.Install(ueNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simDurationSeconds));

    // Animation output
    AnimationInterface anim("lte-network.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Assign node colors in animation
    anim.UpdateNodeDescription(ueNodes.Get(0), "UE-0");
    anim.UpdateNodeDescription(ueNodes.Get(1), "UE-1");
    anim.UpdateNodeDescription(enbNode.Get(0), "eNB");
    anim.UpdateNodeColor(ueNodes.Get(0), 255, 0, 0);   // Red
    anim.UpdateNodeColor(ueNodes.Get(1), 0, 0, 255);   // Blue
    anim.UpdateNodeColor(enbNode.Get(0), 0, 255, 0);   // Green

    // Start simulation
    Simulator::Stop(Seconds(simDurationSeconds));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
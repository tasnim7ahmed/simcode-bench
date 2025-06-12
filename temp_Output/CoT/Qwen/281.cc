#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    // Create nodes: 1 eNodeB, 3 UEs
    NodeContainer enbNodes;
    enbNodes.Create(1);

    NodeContainer ueNodes;
    ueNodes.Create(3);

    // Install LTE devices
    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach UEs to eNodeB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);

    // Assign IP addresses
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set default gateway for UEs (toward PGW)
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ueNode->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Setup UDP server on eNodeB
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    // Setup UDP clients on UEs
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.0);
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(ueIpIface.GetAddress(i), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(10));
        echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
        echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = echoClient.Install(ueNodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(10.0));
    }

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)),
                              "Distance", DoubleValue(5.0),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2]"));
    mobility.Install(ueNodes);

    // Enable logging for mobility
    LogComponentEnable("RandomWalk2dMobilityModel", LOG_LEVEL_INFO);

    // Enable data rate tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("lte-udp-mobility.tr");
    lteHelper->EnablePhyTraces();
    lteHelper->EnableMacTraces();
    lteHelper->EnableRlcTraces();
    epcHelper->EnableMuxDemuxTraces(stream);

    // Set simulation time and run
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("LteTcpSimulation", LOG_LEVEL_INFO);

    // Create nodes: 2 UEs and 1 eNodeB
    NodeContainer ueNodes;
    ueNodes.Create(2);

    NodeContainer enbNode;
    enbNode.Create(1);

    // Install LTE/EPC network
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::PfFfMacScheduler");

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNode);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Attach all UEs to the first eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        lteHelper->Attach(ueDevs.Get(i), enbDevs.Get(0));
    }

    // Install internet stack on UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Set TCP as default protocol for sockets
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

    // Setup applications
    uint16_t dlPort = 10000;
    uint16_t ulPort = 20000;

    // Server application on UE 0
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(ueNodes.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(10.0));

    // Client application on UE 1
    Address remoteAddress(InetSocketAddress(ueIpIfaces.GetAddress(0), dlPort));
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", remoteAddress);
    bulkSend.SetAttribute("MaxBytes", UintegerValue(0)); // unlimited
    ApplicationContainer clientApp = bulkSend.Install(ueNodes.Get(1));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(10.0));

    // Mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(ueNodes);
    mobility.Install(enbNode);

    // Animation output
    AnimationInterface anim("lte-tcp-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    // Assign node colors in animation
    anim.UpdateNodeDescription(enbNode.Get(0), "eNodeB");
    anim.UpdateNodeColor(enbNode.Get(0), 255, 0, 0); // red

    for (uint32_t i = 0; i < ueNodes.GetN(); ++i) {
        std::ostringstream oss;
        oss << "UE-" << i;
        anim.UpdateNodeDescription(ueNodes.Get(i), oss.str());
        anim.UpdateNodeColor(ueNodes.Get(i), 0, 0, 255); // blue
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
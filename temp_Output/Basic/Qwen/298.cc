#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteHandoverSimulation");

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("LteHandoverSimulation", LOG_LEVEL_INFO);

    // Simulation time
    double simTime = 10.0;
    Config::SetDefault("ns3::ConfigStore::Filename", StringValue("input-defaults.txt"));
    Config::SetDefault("ns3::ConfigStore::Mode", StringValue("Load"));
    Config::SetDefault("ns3::ConfigStore::FileFormat", StringValue("Xml"));

    // Create eNodeBs and UE nodes
    NodeContainer enbNodes;
    enbNodes.Create(2);

    NodeContainer ueNodes;
    ueNodes.Create(1);

    // Install LTE stack on eNodeBs and UEs
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    lteHelper->SetAttribute("UseRlcSm", BooleanValue(true));

    // Point-to-point EPC helpers
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Set handover algorithm
    lteHelper->SetHandoverAlgorithmType("ns3::A3RsRpHandoverAlgorithm");
    lteHelper->SetHandoverAlgorithmAttribute("TimeToTrigger", TimeValue(MilliSeconds(256)));

    NetDeviceContainer enbDevs;
    enbDevs = lteHelper->InstallEnbDevice(enbNodes);

    NetDeviceContainer ueDevs;
    ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIfaces;
    ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Attach each UE to a random eNB
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u) {
        Ptr<Node> ueNode = ueNodes.Get(u);
        Ptr<LteUeNetDevice> lteUeDev = ueNode->GetDevice(0)->GetObject<LteUeNetDevice>();
        NS_ASSERT(lteUeDev != nullptr);
        lteHelper->Attach(lteUeDev, enbDevs.Get(0));
    }

    // Setup mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 50.0)),
                              "Distance", DoubleValue(1.0));
    mobility.Install(ueNodes);

    // Install applications
    uint16_t dlPort = 1234;
    uint16_t ulPort = 2000;
    uint16_t otherPort = 3000;

    // UDP server on first eNB
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer dlSinkApps = dlPacketSinkHelper.Install(ueNodes.Get(0));
    dlSinkApps.Start(Seconds(0.0));
    dlSinkApps.Stop(Seconds(simTime));

    // UDP client on UE
    InetSocketAddress dst = InetSocketAddress(ueIpIfaces.GetAddress(0), dlPort);
    dst.SetTos(0xb8); // AC_BE
    UdpClientHelper dlClient(ueIpIfaces.GetAddress(0), dlPort);
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000000));
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(1)));
    dlClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = dlClient.Install(enbNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime - 0.1));

    // Connect simulation traces
    lteHelper->EnableTraces();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteUeRandomWalk");

int main(int argc, char* argv[]) {
    // Enable logging
    LogComponentEnable("LteUeRandomWalk", LOG_LEVEL_INFO);

    // Set simulation parameters
    uint16_t numberOfUes = 1;
    double simulationTime = 10.0;
    double interPacketInterval = 1.0; // Seconds

    // Create LTE helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Create EPC helper
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create remote host node
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create the Internet
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer internetDevices = p2p.Install(remoteHostContainer, epcHelper->GetSpGateway());
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

    // RoutingHelper
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Create eNodeB node
    NodeContainer enbNodes;
    enbNodes.Create(1);
    // Install LTE Devices to the eNodeB
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);

    // Create UE nodes
    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);
    // Install LTE Devices to the UEs
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set active protocol to be used by the PGW (required for E2E tracing to work)
    Ptr<EpcTftClassifier> tft = Create<EpcTftClassifier>();
    tft->SetFilterUdpDestPort(80);
    epcHelper->AddUeTft(ueIpIface.GetAddress(0), tft);

    // Attach UE to the closest eNodeB
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    // Activate a data radio bearer between UE and eNodeB
    enum EpsBearer::Qci q = EpsBearer::GBR_CONV_VOICE;
    EpsBearer bearer(q);
    lteHelper->ActivateDataRadioBearer(ueNodes, bearer, enbLteDevs.Get(0));

    // Set mobility model
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);
    Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
    enbNodes.Get(0)->AggregateObject(enbMobility);
    Vector enbPosition(25.0, 25.0, 0.0); // Center of the area
    enbMobility->SetPosition(enbPosition);

    // Install and start applications on UE and eNodeB
    uint16_t dlPort = 80;

    // Server (eNodeB)
    UdpServerHelper dlServer(dlPort);
    ApplicationContainer dlServerApps = dlServer.Install(remoteHost);
    dlServerApps.Start(Seconds(0.0));
    dlServerApps.Stop(Seconds(simulationTime));

    // Client (UE)
    UdpClientHelper ulClient(remoteHostAddr, dlPort);
    ulClient.SetAttribute("MaxPackets", UintegerValue(5));
    ulClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    ulClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer ulClientApps = ulClient.Install(ueNodes);
    ulClientApps.Start(Seconds(1.0));
    ulClientApps.Stop(Seconds(simulationTime));

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();
    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create EPC helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host (internet)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Configure the remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    NetDeviceContainer internetDevices = p2ph.Install(remoteHostContainer, epcHelper->GetSpGateway());
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // Create gNB node
    NodeContainer gnbContainer;
    gnbContainer.Create(1);

    // Create UE node
    NodeContainer ueContainer;
    ueContainer.Create(1);

    // Mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", DoubleValue(500.0),
                                  "Y", DoubleValue(500.0),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=10]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gnbContainer);

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", RectangleValue(Rectangle(0, 1000, 0, 1000)));
    mobility.Install(ueContainer);

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(gnbContainer);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueContainer);

    // Attach the UE to the eNB
    lteHelper->Attach(ueLteDevs, enbLteDevs.Get(0));

    // Set up the IP addresses
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Set the default gateway for the UE
    Ptr<Node> ue = ueContainer.Get(0);
    Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting(ue->GetObject<Ipv4>());
    ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);

    // Install application
    uint16_t dlPort = 2000;
    UdpClientHelper dlClient(ueIpIface.GetAddress(0), dlPort);
    dlClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
    dlClient.SetAttribute("MaxPackets", UintegerValue(1000));
    dlClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = dlClient.Install(remoteHost);
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(10.0));

    UdpServerHelper dlPacketSink(dlPort);
    ApplicationContainer serverApps = dlPacketSink.Install(ue);
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();
    return 0;
}
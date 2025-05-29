#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create Nodes: one gNB and two UEs
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Create LTE Helper
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();

    // Set up LTE EPC
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create a remote host node (optional in this simple scenario)
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // PointToPoint link between remoteHost and EPC
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2ph.SetDeviceAttribute("Delay", StringValue("1ms"));
    NetDeviceContainer remoteHostNetDev = p2ph.Install(remoteHostContainer, epcHelper->Get ব্যবস্থাপক () );
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer remoteHostInterface = ipv4h.Assign(remoteHostNetDev);
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4GlobalRouting> globalRouting = ipv4RoutingHelper.CreateGlobalRouting();
    globalRouting->AddInterface(remoteHostInterface.GetAddress(0), remoteHostInterface.GetMask(0));

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(gnbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach one UE per eNodeB
    for (uint32_t i = 0; i < ueNodes.GetN(); i++) {
        lteHelper->Attach(ueLteDevs.Get(i), gnbNodes.Get(0));
    }

    // Set mobility model for the UEs
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 50, 0, 50)));
    mobility.Install(ueNodes);

    // Install and start the TCP server on UE2
    uint16_t port = 8080;
    Address serverAddress = InetSocketAddress(ueIpIface.GetAddress(1), port);
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", serverAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install and start the TCP client on UE1, targeting UE2
    BulkSendHelper bulkSendHelper("ns3::TcpSocketFactory", serverAddress);
    bulkSendHelper.SetAttribute("MaxBytes", UintegerValue(1024000));
    ApplicationContainer clientApp = bulkSendHelper.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Start the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
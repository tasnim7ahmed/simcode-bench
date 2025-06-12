#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nr-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation parameters
    double simTime = 10.0;
    uint32_t udpPacketSize = 1024;
    uint32_t numUdpPackets = 1000;
    double interPacketInterval = 0.01; // seconds

    // Log components
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);

    // Create UE and gNB nodes
    NodeContainer ueNodes;
    NodeContainer enbNodes;
    ueNodes.Create(1);
    enbNodes.Create(1);

    // Create the 5G Core Network (CN) node
    NodeContainer coreNetworkNode;
    coreNetworkNode.Create(1);

    // Install Internet stack on all nodes
    InternetStackHelper internet;
    internet.Install(ueNodes);
    internet.Install(enbNodes);
    internet.Install(coreNetworkNode);

    // Setup P2P link between gNB and CN
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2ph.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer enbCoreDevices = p2ph.Install(enbNodes.Get(0), coreNetworkNode.Get(0));

    // Assign IP addresses to gNB-CN link
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer enbCoreInterfaces = ipv4.Assign(enbCoreDevices);

    // Setup 5G NR links
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<PointToPointEpcHelper> p2pEpcHelper = DynamicCast<PointToPointEpcHelper>(epcHelper);

    // Install LTE devices: UEs and gNB
    NrHelper nrHelper;
    nrHelper.SetEpcHelper(p2pEpcHelper);
    nrHelper.SetSchedulerType("ns3::NrMacSchedulerTdma");

    NetDeviceContainer enbDevs = nrHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = nrHelper.InstallUeDevice(ueNodes);

    // Attach UE to gNB
    p2pEpcHelper->AddEnb(enbNodes.Get(0), enbDevs.Get(0), 0, 0);
    p2pEpcHelper->AddUe(ueDevs.Get(0), 0);
    p2pEpcHelper->AttachUe(ueNodes.Get(0), 0);

    // Assign IP address to UE
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address ueAddr = ueIpIface.GetAddress(0);

    // Get gNB address from the EPC side
    Ipv4Address gnbAddr = enbCoreInterfaces.GetAddress(0);

    // Install UDP Echo Server on UE at port 9
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApp = echoServer.Install(ueNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Install UDP Echo Client on gNB
    UdpEchoClientHelper echoClient(ueAddr, 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(numUdpPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(udpPacketSize));

    ApplicationContainer clientApp = echoClient.Install(coreNetworkNode.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simTime));

    // Enable logging for applications
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/applications-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store.h"
#include "ns3/epc-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Enable logging
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // LTE and EPC helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    // Create eNB and UE nodes
    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(1);

    // Install Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    // Install LTE Devices
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install Internet stack on UEs and EPC’s remote host
    InternetStackHelper internet;
    internet.Install(ueNodes);

    // Get remote host from EPC
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    internet.Install(remoteHostContainer);

    // Create Internet backhaul between remote host and PGW
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);

    // Assign IP addresses
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIfaces.GetAddress(1);

    // Assign UE IP address
    Ipv4InterfaceContainer ueIpIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));
    Ipv4Address ueIpAddr = ueIpIfaces.GetAddress(0);

    // Attach UE to eNB
    lteHelper->Attach(ueDevs.Get(0), enbDevs.Get(0));

    // Allow remote to reach UE
    // (Not strictly necessary here since eNB serves as both LTE and server endpoint)

    // Install UDP server on eNB node (acts as UDP echo server)
    uint16_t serverPort = 9;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(enbNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    // Install UDP client on UE node, targeting eNB
    UdpClientHelper udpClient(ueIpAddr, serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(100)); // upper bound
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = udpClient.Install(ueNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    // Set simulation attributes
    uint32_t numberOfUes = 1;
    double simulationTime = 10.0;
    double interPacketInterval = 0.01;

    // Create LTE helper
    LteHelper lteHelper;

    // Create EPC helper
    Ptr<LteEpcHelper> epcHelper = CreateObject<LteEpcHelper>();
    lteHelper.SetEpcHelper(epcHelper);

    // Create remote host node
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Configure EPC
    EpcTaps::epc_node_config_t config;
    config.sgi_tx_queue_size = 10000;
    epcHelper->Configure(config);

    // Create PGW
    NetDeviceContainer pgwNetDevice = epcHelper->InstallPgwDevice(remoteHostContainer);

    // Assign IP address to remote host
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(pgwNetDevice);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(0);

    // Add route to remote host
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create eNB node
    NodeContainer enbNodes;
    enbNodes.Create(1);
    Ptr<Node> enbNode = enbNodes.Get(0);

    // Mobility model for eNB
    Ptr<ConstantPositionMobilityModel> enbMobility = CreateObject<ConstantPositionMobilityModel>();
    enbNode->AggregateObject(enbMobility);
    enbMobility->SetPosition(Vector(0.0, 0.0, 0.0));

    // Create UE nodes
    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);

    // Mobility model for UE
    MobilityHelper ueMobility;
    ueMobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                     "X", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
                                     "Y", StringValue("ns3::UniformRandomVariable[Min=1.0|Max=10.0]"),
                                     "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    ueMobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                  "Mode", StringValue("Time"),
                                  "Time", StringValue("1s"),
                                  "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    ueMobility.Install(ueNodes);

    // Install LTE devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper.InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper.InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach the UEs to the closest eNB
    lteHelper.Attach(ueLteDevs, enbLteDevs.Get(0));

    // Install application
    uint16_t dlPort = 10000;
    ApplicationContainer clientApps;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Node> ue = ueNodes.Get(u);
        Ipv4Address ueAddress = ueIpIface.GetAddress(u);

        UdpClientHelper client(ueAddress, dlPort);
        client.SetAttribute("Interval", TimeValue(Seconds(interPacketInterval)));
        client.SetAttribute("MaxPackets", UintegerValue(1000000));
        client.SetAttribute("PacketSize", UintegerValue(1024););
        clientApps.Add(client.Install(remoteHost));
    }

    ApplicationContainer serverApps;
    UdpServerHelper dlPacketSinkHelper(dlPort);
    serverApps.Add(dlPacketSinkHelper.Install(ueNodes));

    serverApps.Start(Seconds(0.0));
    clientApps.Start(Seconds(0.0));

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();

    return 0;
}
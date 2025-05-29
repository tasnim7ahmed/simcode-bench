#include <iostream>
#include <fstream>
#include <string>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/config-store.h"
#include "ns3/epc-helper.h"
#include "ns3/point-to-point-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LteTcpNetAnim");

int main(int argc, char *argv[]) {
    // Set up command line arguments
    uint16_t numberOfUes = 2;
    double simulationTime = 10.0;
    std::string phyMode = "UeRealityModel";

    CommandLine cmd;
    cmd.AddValue("numberOfUes", "Number of UEs", numberOfUes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("phyMode", "PHY mode (UeRealityModel or other)", phyMode);
    cmd.Parse(argc, argv);

    // Enable logging
    LogComponentEnable("LteTcpNetAnim", LOG_LEVEL_INFO);

    // Create Helpers
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<EpcHelper> epcHelper = CreateObject<EpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Create EPC nodes
    Ptr<Node> pgw = epcHelper->GetPgwNode();

    // Create Remote Host
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // Create the internet link towards the remote host
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.010)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

    // Set routing for the remote host
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Create eNodeB
    NodeContainer enbNodes;
    enbNodes.Create(1);
    
    // Configure eNodeB
    lteHelper->SetEnbDeviceAttribute("DlEarfcn", UintegerValue(9660));
    lteHelper->SetEnbDeviceAttribute("UlEarfcn", UintegerValue(18160));

    // Create UEs
    NodeContainer ueNodes;
    ueNodes.Create(numberOfUes);

    // Configure UEs
    lteHelper->SetUeDeviceAttribute("UeTxPower",IntegerValue(23));
    lteHelper->SetUeDeviceAttribute("DlEarfcn", UintegerValue(9660));
    lteHelper->SetUeDeviceAttribute("UlEarfcn", UintegerValue(18160));

    // Install LTE Devices to the nodes
    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    // Install the IP stack on the UEs
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));

    // Attach one UE per eNodeB
    for (uint16_t i = 0; i < ueNodes.GetN(); i++) {
        lteHelper->Attach(ueLteDevs.Get(i), enbLteDevs.Get(0));
    }

    // Set active connections
    lteHelper->ActivateDedicatedBearer(ueLteDevs, EpsBearer(EpsBearer::GBR_CONV_VOICE));

    // Create Applications
    uint16_t dlPort = 5000;
    uint16_t ulPort = 5000;

    // Install TCP Receiver on the remote host
    PacketSinkHelper dlPacketSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), dlPort));
    ApplicationContainer dlPacketSinkApp = dlPacketSinkHelper.Install(remoteHost);
    dlPacketSinkApp.Start(Seconds(1));
    dlPacketSinkApp.Stop(Seconds(simulationTime));

    // Install TCP Transmitter on the UEs
    for (uint16_t i = 0; i < ueNodes.GetN(); i++) {
        AddressValue remoteAddress(InetSocketAddress(remoteHostAddr, dlPort));
        Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
        BulkSendHelper ulBulkSendHelper("ns3::TcpSocketFactory", remoteAddress);
        ulBulkSendHelper.SetAttribute("SendSize", UintegerValue(1500));
        ApplicationContainer ulBulkSendApp = ulBulkSendHelper.Install(ueNodes.Get(i));
        ulBulkSendApp.Start(Seconds(2));
        ulBulkSendApp.Stop(Seconds(simulationTime));
    }

    // Add NetAnim
    AnimationInterface anim("lte-tcp-netanim.xml");
    anim.SetConstantPosition(enbNodes.Get(0), 10.0, 20.0);
    anim.SetConstantPosition(ueNodes.Get(0), 20.0, 10.0);
    anim.SetConstantPosition(ueNodes.Get(1), 20.0, 30.0);
    anim.SetConstantPosition(remoteHost, 30.0, 20.0);

    // Run the simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();
    return 0;
}
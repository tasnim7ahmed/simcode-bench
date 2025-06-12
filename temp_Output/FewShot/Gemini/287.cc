#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/mmwave-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    // Configure logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create Nodes: gNB and two UEs
    NodeContainer gnbNodes;
    gnbNodes.Create(1);
    NodeContainer ueNodes;
    ueNodes.Create(2);

    // Configure Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("25.0"),
                                  "Y", StringValue("25.0"),
                                  "Z", StringValue("0.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=25.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                              "Bounds", StringValue("50x50"));
    mobility.Install(ueNodes);

    // Make gNB stationary
    MobilityHelper gnbMobility;
    gnbMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gnbMobility.Install(gnbNodes);
    gnbNodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(25.0, 25.0, 0.0));

    // Configure 5G NR network
    MmWaveHelper mmwaveHelper;
    mmwaveHelper.SetGnbAntennaModelType("ns3::CosineAntennaModel");

    // Install 5G Devices
    NetDeviceContainer gnbDevs = mmwaveHelper.InstallGnb(gnbNodes);
    NetDeviceContainer ueDevs = mmwaveHelper.InstallUe(ueNodes);

    // Configure EPC
    Ptr<LteEpcHelper> epcHelper = CreateObject<LteEpcHelper>();
    mmwaveHelper.SetEpcHelper(epcHelper);

    // Assign IP addresses
    InternetStackHelper internet;
    internet.Install(gnbNodes);
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    Ipv4AddressHelper ipGnb;
    ipGnb.SetBase("1.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer gnbIpIface = ipGnb.Assign(gnbDevs);

    // Set up UDP server on UE 1
    uint16_t port = 5001;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(ueNodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP client on UE 0
    UdpClientHelper client(ueIpIface.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xffffffff); //send packets until the end of the simulation
    client.SetAttribute("Interval", TimeValue(MilliSeconds(10)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(ueNodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Activate default EPS bearer
    enum EpsBearer::Qci q = EpsBearer::GBR_QCI_BEARER;
    EpsBearer bearer(q);
    mmwaveHelper.ActivateDataRadioBearer(ueNodes, bearer);

    // Enable PCAP tracing
    mmwaveHelper.EnablePcap("mmwave-simulation", gnbDevs.Get(0));
    
    // Run the simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    Simulator::Destroy();
    return 0;
}
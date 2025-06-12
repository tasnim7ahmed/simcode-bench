#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/sumo-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    uint32_t numVehicles = 10;
    std::string sumoConfig = "vanet.sumocfg";
    double simTime = 80.0;

    CommandLine cmd;
    cmd.AddValue("numVehicles", "Number of vehicle nodes", numVehicles);
    cmd.AddValue("sumoConfig", "SUMO Configuration file", sumoConfig);
    cmd.Parse(argc, argv);

    // Enable logging as needed
    // LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create vehicle nodes
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // SUMO Mobility (ns3-sumo-interface)
    Ptr<SumoInterface> sumo = CreateObject<SumoInterface> ();
    sumo->SetConfigFile (sumoConfig);
    sumo->SetAttribute ("NumNodes", UintegerValue (numVehicles));
    sumo->Install (vehicles);

    // Wi-Fi configuration
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211p);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set ("TxPowerStart", DoubleValue (20.0));
    wifiPhy.Set ("TxPowerEnd", DoubleValue (20.0));
    wifiPhy.Set ("RxGain", DoubleValue (0));
    wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-79));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel (wifiChannel.Create ());

    NqosWaveMacHelper wifiMac = NqosWaveMacHelper::Default ();

    NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, vehicles);

    // Internet stack with DSR
    InternetStackHelper internet;
    DsrMainHelper dsrMain;
    DsrHelper dsrHelper;
    internet.Install (vehicles);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Install DSR routing
    dsrMain.Install (dsrHelper, vehicles);

    // UDP Echo Server (on node 0)
    uint16_t echoPort = 9;
    UdpEchoServerHelper echoServer(echoPort);
    ApplicationContainer serverApps = echoServer.Install(vehicles.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simTime - 1.0));

    // UDP Echo Client (on node numVehicles-1)
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), echoPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(vehicles.Get(numVehicles - 1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simTime - 1.0));

    // Enable PCAP tracing
    wifiPhy.EnablePcapAll("vanet-wifi");

    // NetAnim
    AnimationInterface anim("vanet.xml");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
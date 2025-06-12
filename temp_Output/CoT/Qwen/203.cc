#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    uint32_t numVehicles = 5;
    double simDuration = 20.0;
    double dataRate = 2; // Mbps
    uint16_t serverPort = 9;
    Time interPacketInterval = Seconds(1.0);

    CommandLine cmd(__FILE__);
    cmd.AddValue("numVehicles", "Number of vehicles in the simulation", numVehicles);
    cmd.AddValue("simDuration", "Duration of simulation in seconds", simDuration);
    cmd.AddValue("dataRate", "Data rate in Mbps", dataRate);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));

    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6MbpsBW10MHz"),
                                 "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiChannel.Create(), vehicles);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        double speed = 10 + (i * 2); // Speed between 10 and 18 m/s
        mob->SetAttribute("Velocity", Vector3DValue(Vector(speed, 0, 0)));
    }

    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    UdpEchoServerHelper echoServer(serverPort);
    ApplicationContainer serverApps = echoServer.Install(vehicles.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), serverPort);
    echoClient.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
    echoClient.SetAttribute("Interval", TimeValue(interPacketInterval));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numVehicles; ++i) {
        clientApps.Add(echoClient.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simDuration - 0.1));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("vanet_simulation.tr");
    wifi.EnableAsciiAll(stream);

    AnimationInterface anim("vanet_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
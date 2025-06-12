#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularNetworkSimulation");

int main(int argc, char *argv[]) {
    uint32_t nVehicles = 4;
    double simDuration = 10.0;
    double packetInterval = 1.0;
    double dataRate = 500; // kbps

    CommandLine cmd(__FILE__);
    cmd.AddValue("nVehicles", "Number of vehicles in the simulation", nVehicles);
    cmd.AddValue("simDuration", "Duration of the simulation in seconds", simDuration);
    cmd.AddValue("packetInterval", "Interval between UDP packets (seconds)", packetInterval);
    cmd.AddValue("dataRate", "Data rate for UDP traffic in kbps", dataRate);
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(nVehicles);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6Mbps"), "ControlMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, vehicles);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    for (uint32_t i = 0; i < nVehicles; ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        Vector position = Vector(10.0 + i * 20.0, 0.0, 0.0);
        mob->SetPosition(position);
        mob->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s along x-axis
    }

    InternetStackHelper stack;
    stack.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetBroadcast(), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate * 1000)));

    ApplicationContainer apps;
    for (uint32_t i = 0; i < nVehicles; ++i) {
        for (uint32_t j = 0; j < nVehicles; ++j) {
            if (i != j) {
                onoff.SetAttribute("Remote", AddressValue(InetSocketAddress(interfaces.GetAddress(j), port)));
                apps = onoff.Install(vehicles.Get(i));
                apps.Start(Seconds(1.0));
                apps.Stop(Seconds(simDuration));
            }
        }
    }

    Config::SetDefault("ns3::Ipv4L3Protocol::EnableUdpChecksums", BooleanValue(true));
    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinks;
    for (uint32_t i = 0; i < nVehicles; ++i) {
        sinks = sink.Install(vehicles.Get(i));
        sinks.Start(Seconds(0.0));
        sinks.Stop(Seconds(simDuration));
    }

    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("vehicular-wifi.tr"));

    AnimationInterface anim("vehicular-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
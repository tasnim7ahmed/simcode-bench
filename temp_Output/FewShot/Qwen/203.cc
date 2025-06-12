#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 20.0;
    uint32_t numVehicles = 5;
    uint16_t port = 8080;

    // Enable 802.11p
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate6Mbps"));
    Config::SetDefault("ns3::WifiPhy::ChannelNumber", UintegerValue(36));
    Config::SetDefault("ns3::WifiMac::ShortSlotTimeSupported", BooleanValue(false));

    // Create nodes (vehicles)
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Setup mobility for a straight road
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

    // Assign speeds to each vehicle
    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<MobilityModel> model = node->GetObject<MobilityModel>();
        double speed = 10 + i * 2; // Varying speeds from 10 m/s to 18 m/s
        model->SetAttribute("Velocity", VectorValue(Vector(speed, 0, 0)));
    }

    // Configure Wi-Fi 802.11p
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    wifiMac.SetType("ns3::AdhocWifiMac");
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, vehicles);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(wifiDevices);

    // Set up UDP server on vehicle 0
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(vehicles.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simTime));

    // Set up UDP clients on other vehicles
    for (uint32_t i = 1; i < numVehicles; ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1000));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(vehicles.Get(i));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(simTime));
    }

    // Enable PDR and delay logging
    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::UdpEchoServer/TraceRtt",
                    MakeCallback([](Ptr<const Application> app, const SequenceNumber32& oldVal, const SequenceNumber32& newVal) {
                        std::cout << Simulator::Now().GetSeconds() << "s RTT: " << newVal - oldVal << " seq: " << newVal << std::endl;
                    }));

    // Setup animation
    AnimationInterface anim("vanet_simulation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));
    anim.EnablePacketMetadata(true);

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMCSExperiment");

int main(int argc, char *argv[]) {
    std::string phyMode("Spectrum");
    double distance = 1.0; // meters
    double simulationTime = 10; // seconds
    uint8_t mcsValue = 3;
    uint16_t channelWidth = 20;
    bool shortGuardInterval = false;
    bool useYans = true;
    std::string wifiType = "yans";
    std::string errorModelType = "ns3::NistErrorRateModel";

    CommandLine cmd(__FILE__);
    cmd.AddValue("phyMode", "Either 'Spectrum' or 'Yans'", phyMode);
    cmd.AddValue("distance", "Distance in meters between STA and AP", distance);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("mcs", "MCS index value", mcsValue);
    cmd.AddValue("channelWidth", "Channel width in MHz (20, 40, 80)", channelWidth);
    cmd.AddValue("shortGuardInterval", "Use short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue("wifiType", "Wi-Fi PHY type: 'yans' or 'spectrum'", wifiType);
    cmd.AddValue("errorModel", "Error model type", errorModelType);
    cmd.Parse(argc, argv);

    useYans = (wifiType == "yans");

    NodeContainer nodes;
    nodes.Create(2);

    // AP is node 0, STA is node 1
    Ptr<Node> apNode = nodes.Get(0);
    Ptr<Node> staNode = nodes.Get(1);

    // Mobility setup
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    WifiMacHelper wifiMac;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("HtMcs" + std::to_string(mcsValue)),
                                 "ControlMode", StringValue("HtMcs" + std::to_string(mcsValue)));

    if (useYans) {
        YansWifiPhyHelper wifiPhy;
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
        wifiPhy.SetChannel(channel.Create());
        wifiPhy.SetErrorRateModel(errorModelType);
        wifiPhy.Set("ChannelWidth", UintegerValue(channelWidth));
        wifiPhy.Set("ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
        wifiMac.SetType("ns3::ApWifiMac");
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);
        wifiMac.SetType("ns3::StaWifiMac");
        NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, staNode);
    } else {
        SpectrumWifiPhyHelper wifiPhy;
        wifiPhy.SetPcapDataLinkType(SpectrumWifiPhyHelper::DLT_IEEE802_11_RADIO);
        Ptr<MultiModelSpectrumChannel> channel = CreateObject<MultiModelSpectrumChannel>();
        Ptr<FriisPropagationLossModel> lossModel = CreateObject<FriisPropagationLossModel>();
        channel->AddPropagationLossModel(lossModel);
        Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
        channel->SetPropagationDelayModel(delayModel);
        wifiPhy.SetChannel(channel);
        wifiPhy.SetErrorRateModel(errorModelType);
        wifiPhy.Set("ChannelWidth", UintegerValue(channelWidth));
        wifiPhy.Set("ShortGuardIntervalSupported", BooleanValue(shortGuardInterval));
        wifiMac.SetType("ns3::ApWifiMac");
        NetDeviceContainer apDevice = wifi.Install(wifiPhy, wifiMac, apNode);
        wifiMac.SetType("ns3::StaWifiMac");
        NetDeviceContainer staDevice = wifi.Install(wifiPhy, wifiMac, staNode);
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(nodes.Get(0)->GetDevice(0));
    Ipv4InterfaceContainer staInterface = address.Assign(nodes.Get(1)->GetDevice(0));

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(staNode);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper client(staInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(apNode);
    clientApps.Start(Seconds(0.1));
    clientApps.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    uint64_t totalBytesRx = StaticCast<UdpServer>(serverApps.Get(0))->GetReceived());
    double throughput = (totalBytesRx * 8) / (simulationTime * 1000000.0); // Mbps

    std::cout << "MCS=" << static_cast<uint32_t>(mcsValue)
              << ", ChannelWidth=" << channelWidth
              << ", ShortGI=" << shortGuardInterval
              << ", Throughput(Mbps)=" << throughput
              << ", PhyType=" << wifiType
              << ", ErrorModel=" << errorModelType
              << std::endl;

    Simulator::Destroy();

    return 0;
}
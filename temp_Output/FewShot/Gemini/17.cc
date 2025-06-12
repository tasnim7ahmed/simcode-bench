#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/command-line.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    bool enableRtsCts = false;
    uint32_t payloadSize = 1472;
    double simulationTime = 10;
    double distance = 5.0;

    CommandLine cmd;
    cmd.AddValue("enableRtsCts", "Enable or disable RTS/CTS (0=disable, 1=enable)", enableRtsCts);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between AP and station", distance);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode",
                       StringValue("ns3::ConstantRateWifiManager"));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);

    YansWifiChannelHelper channel36 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy36 = YansWifiPhyHelper::Default();
    phy36.SetChannel(channel36.Create());
    phy36.Set("ChannelNumber", UintegerValue(36));

    YansWifiChannelHelper channel40 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy40 = YansWifiPhyHelper::Default();
    phy40.SetChannel(channel40.Create());
    phy40.Set("ChannelNumber", UintegerValue(40));

    YansWifiChannelHelper channel44 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy44 = YansWifiPhyHelper::Default();
    phy44.SetChannel(channel44.Create());
    phy44.Set("ChannelNumber", UintegerValue(44));

    YansWifiChannelHelper channel48 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy48 = YansWifiPhyHelper::Default();
    phy48.SetChannel(channel48.Create());
    phy48.Set("ChannelNumber", UintegerValue(48));

    WifiMacHelper mac;
    Ssid ssid[4];
    for (int i = 0; i < 4; ++i) {
        ssid[i] = Ssid("ns3-wifi-" + std::to_string(i));
    }

    NodeContainer apNodes[4];
    NodeContainer staNodes[4];
    NetDeviceContainer apDevices[4];
    NetDeviceContainer staDevices[4];

    for (int i = 0; i < 4; ++i) {
        apNodes[i].Create(1);
        staNodes[i].Create(1);

        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid[i]),
                    "BeaconInterval", TimeValue(MilliSeconds(100)));

        apDevices[i] = wifi.Install(phy36 + i*4, mac, apNodes[i]);

        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid[i]),
                    "ActiveProbing", BooleanValue(false));

        staDevices[i] = wifi.Install(phy36 + i*4, mac, staNodes[i]);
    }

    // Configure aggregation settings
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmsduEnable", BooleanValue(false));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmpduEnable", BooleanValue(true));
    Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/MaxAmpduSize", UintegerValue(65535)); //65KB

    Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmsduEnable", BooleanValue(false));
    Config::Set("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmpduEnable", BooleanValue(false));

    Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmsduEnable", BooleanValue(true));
    Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmpduEnable", BooleanValue(false));
    Config::Set("/NodeList/2/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmsduMaxBytes", UintegerValue(8192)); //8KB

    Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmsduEnable", BooleanValue(true));
    Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmpduEnable", BooleanValue(true));
    Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/AmsduMaxBytes", UintegerValue(4096)); //4KB
    Config::Set("/NodeList/3/DeviceList/*/$ns3::WifiNetDevice/Mac/EdcaQueue[0]/MaxAmpduSize", UintegerValue(32768)); //32KB

    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(0));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(distance),
                                  "GridWidth", UintegerValue(1),
                                  "LayoutType", StringValue("RowFirst"));

    for(int i=0; i<4; ++i){
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(apNodes[i]);

        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(staNodes[i]);
    }

    InternetStackHelper internet;
    for(int i=0; i<4; ++i){
        internet.Install(apNodes[i]);
        internet.Install(staNodes[i]);
    }

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterfaces[4];
    Ipv4InterfaceContainer staInterfaces[4];

    address.SetBase("10.1.1.0", "255.255.255.0");
    staInterfaces[0] = address.Assign(staDevices[0]);
    apInterfaces[0] = address.Assign(apDevices[0]);

    address.SetBase("10.1.2.0", "255.255.255.0");
    staInterfaces[1] = address.Assign(staDevices[1]);
    apInterfaces[1] = address.Assign(apDevices[1]);

    address.SetBase("10.1.3.0", "255.255.255.0");
    staInterfaces[2] = address.Assign(staDevices[2]);
    apInterfaces[2] = address.Assign(apDevices[2]);

    address.SetBase("10.1.4.0", "255.255.255.0");
    staInterfaces[3] = address.Assign(staDevices[3]);
    apInterfaces[3] = address.Assign(apDevices[3]);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;

    for(int i=0; i<4; ++i){
      UdpClientHelper client(apInterfaces[i].GetAddress(0), port);
      client.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
      client.SetAttribute("Interval", TimeValue(Seconds(0.001)));
      client.SetAttribute("PacketSize", UintegerValue(payloadSize));

      ApplicationContainer clientApps = client.Install(staNodes[i].Get(0));
      clientApps.Start(Seconds(1.0));
      clientApps.Stop(Seconds(simulationTime));

      UdpServerHelper server(port);
      ApplicationContainer serverApps = server.Install(apNodes[i].Get(0));
      serverApps.Start(Seconds(0.0));
      serverApps.Stop(Seconds(simulationTime + 1));
    }

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
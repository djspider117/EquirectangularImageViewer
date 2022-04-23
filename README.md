# Where to get it
Via nuget: https://www.nuget.org/packages/EquirectangularImageViewer.UWP/

Install it via the NuGet package manager or running:
`Install-Package EquirectangularImageViewer.UWP -Version 1.0.3`

# EquirectangularImageViewer
A UWP 360 image viewer that will wrap your image onto a sphere. You have control over pitch/yaw/roll and also FOV.

Here you can find one example of usage.
On Loaded:


XAML:
```xml
<Page x:Class="<YOUR_CLASS_NAME_HERE>"
      xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:d="http://schemas.microsoft.com/expression/blend/2008"
      xmlns:mc="http://schemas.openxmlformats.org/markup-compatibility/2006"
      xmlns:eq="using:EquirectangularImageViewer"
      Loaded="Page_Loaded"
      mc:Ignorable="d">

    <Grid>
        <eq:EquirectangularViewer x:Name="PanoViewer"
                                  ManipulationMode="All"
                                  ManipulationDelta="PanoViewer_ManipulationDelta" />
    </Grid>
</Page>

```

C#

```C#

private async void Page_Loaded(object sender, RoutedEventArgs e)
{
    var file = await StorageFile.GetFileFromApplicationUriAsync(new Uri("<your image file here>"));
    PanoViewer.LoadTexture(file.Path);
}

private void PanoViewer_ManipulationDelta(object sender, ManipulationDeltaRoutedEventArgs e)
{
    var sensitivity = Sensitivity;

    PanoViewer.Yaw -= (float)e.Delta.Translation.X / (float)sensitivity;
    PanoViewer.Pitch -= (float)e.Delta.Translation.Y / (float)sensitivity;
    PanoViewer.Roll += e.Delta.Rotation / (float)sensitivity;
    PanoViewer.FieldOfView *= e.Delta.Scale / (float)sensitivity;
}

```

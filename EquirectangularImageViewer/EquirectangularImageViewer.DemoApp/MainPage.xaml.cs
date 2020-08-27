using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Storage;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace EquirectangularImageViewer.DemoApp
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {

        private int[] _fovs = new int[] { 40, 50, 60, 65, 70, 75, 80 };
        private int _fovIndex;

        public double Sensitivity
        {
            get { return (double)GetValue(SensitivityProperty); }
            set { SetValue(SensitivityProperty, value); }
        }
        public bool IsYawRotationEnabled
        {
            get { return (bool)GetValue(IsYawRotationEnabledProperty); }
            set { SetValue(IsYawRotationEnabledProperty, value); }
        }
        public bool IsRollRotationEnabled
        {
            get { return (bool)GetValue(IsRollRotationEnabledProperty); }
            set { SetValue(IsRollRotationEnabledProperty, value); }
        }

        public bool IsPitchRotationEnabled
        {
            get { return (bool)GetValue(IsPitchRotationEnabledProperty); }
            set { SetValue(IsPitchRotationEnabledProperty, value); }
        }

        public bool IsFieldOfViewDynamic
        {
            get { return (bool)GetValue(IsFieldOfViewDynamicProperty); }
            set { SetValue(IsFieldOfViewDynamicProperty, value); }
        }

        public double StaticFieldOfView
        {
            get { return (double)GetValue(StaticFieldOfViewProperty); }
            set { SetValue(StaticFieldOfViewProperty, value); }
        }

        public Uri Texture
        {
            get { return (Uri)GetValue(TextureProperty); }
            set { SetValue(TextureProperty, value); }
        }

        public double Yaw
        {
            get { return (double)GetValue(YawProperty); }
            set { SetValue(YawProperty, value); }
        }

        public double Pitch
        {
            get { return (double)GetValue(PitchProperty); }
            set { SetValue(PitchProperty, value); }
        }
        public double Roll
        {
            get { return (double)GetValue(RollProperty); }
            set { SetValue(RollProperty, value); }
        }

        public static readonly DependencyProperty YawProperty =
            DependencyProperty.Register("Yaw", typeof(double), typeof(MainPage), new PropertyMetadata(0d, (o, e) => (o as MainPage).OnYawChanged((double)e.NewValue)));
        public static readonly DependencyProperty PitchProperty =
            DependencyProperty.Register("Pitch", typeof(double), typeof(MainPage), new PropertyMetadata(0d, (o, e) => (o as MainPage).OnPitchChanged((double)e.NewValue)));
        public static readonly DependencyProperty RollProperty =
            DependencyProperty.Register("Roll", typeof(double), typeof(MainPage), new PropertyMetadata(0d, (o, e) => (o as MainPage).OnRollChanged((double)e.NewValue)));
        public static readonly DependencyProperty TextureProperty =
            DependencyProperty.Register("Texture", typeof(Uri), typeof(MainPage), new PropertyMetadata(null, (o, e) => (o as MainPage).OnTextureChanged(e)));
        public static readonly DependencyProperty StaticFieldOfViewProperty =
            DependencyProperty.Register("StaticFieldOfView", typeof(double), typeof(MainPage), new PropertyMetadata(60d, (o, e) => (o as MainPage).OnStaticFieldOfViewChanged(e)));
        public static readonly DependencyProperty IsFieldOfViewDynamicProperty =
            DependencyProperty.Register("IsFieldOfViewDynamic", typeof(bool), typeof(MainPage), new PropertyMetadata(false));
        public static readonly DependencyProperty IsYawRotationEnabledProperty =
            DependencyProperty.Register("IsYawRotationEnabled", typeof(bool), typeof(MainPage), new PropertyMetadata(true));
        public static readonly DependencyProperty IsRollRotationEnabledProperty =
            DependencyProperty.Register("IsRollRotationEnabled", typeof(bool), typeof(MainPage), new PropertyMetadata(true));
        public static readonly DependencyProperty IsPitchRotationEnabledProperty =
            DependencyProperty.Register("IsPitchRotationEnabled", typeof(bool), typeof(MainPage), new PropertyMetadata(true));
        public static readonly DependencyProperty SensitivityProperty =
            DependencyProperty.Register("Sensitivity", typeof(double), typeof(MainPage), new PropertyMetadata(20d));


        public MainPage()
        {
            InitializeComponent();
            Loaded += MainPage_Loaded;
        }

        private async void MainPage_Loaded(object sender, RoutedEventArgs e)
        {
            var file = await StorageFile.GetFileFromApplicationUriAsync(new Uri("ms-appx:///Assets/testImage.jpg"));
            Texture = new Uri(file.Path);
        }

        private void OnYawChanged(double newValue)
        {
            if (IsYawRotationEnabled)
                swapChain.Yaw = Convert.ToSingle(newValue);
        }
        private void OnPitchChanged(double newValue)
        {
            if (IsPitchRotationEnabled)
                swapChain.Pitch = Convert.ToSingle(newValue);
        }
        private void OnRollChanged(double newValue)
        {
            if (IsRollRotationEnabled)
                swapChain.Roll = Convert.ToSingle(newValue);
        }

        private void OnStaticFieldOfViewChanged(DependencyPropertyChangedEventArgs e)
        {
            if (swapChain == null) return;
            if (!IsFieldOfViewDynamic)
            {
                swapChain.FieldOfView = Convert.ToSingle((double)e.NewValue);
            }
        }
        private void OnTextureChanged(DependencyPropertyChangedEventArgs e)
        {
            if (swapChain == null) return;
            swapChain.LoadTexture((e.NewValue as Uri).LocalPath);
        }

        private void SwapChain_ManipulationDelta(object sender, ManipulationDeltaRoutedEventArgs e)
        {
            var sensitivity = Sensitivity;

            if (IsYawRotationEnabled)
                swapChain.Yaw -= (float)e.Delta.Translation.X / (float)sensitivity;
            Yaw = swapChain.Yaw;

            if (IsPitchRotationEnabled)
                swapChain.Pitch -= (float)e.Delta.Translation.Y / (float)sensitivity;

            if (IsRollRotationEnabled)
                swapChain.Roll += e.Delta.Rotation / (float)sensitivity;

            if (IsFieldOfViewDynamic)
                swapChain.FieldOfView *= e.Delta.Scale / (float)sensitivity;


            Yaw = swapChain.Yaw;
            Pitch = swapChain.Pitch;
            Roll = swapChain.Roll;
        }

        private void btnTestFov_Click(object sender, RoutedEventArgs e)
        {
            swapChain.FieldOfView = _fovs[_fovIndex];
            _fovIndex++;

            if (_fovIndex >= _fovs.Length)
                _fovIndex = 0;
        }
    }
}
